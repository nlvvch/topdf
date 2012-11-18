#include <node.h>
#include <v8.h>

#ifndef UNIX
    #define UNIX
    #define UNIX_64
#endif

#include <scctype.h>
#include <sccex.h>
#include "topdf.h"

using namespace v8;

void initializeOptions (Handle<Object> source, topdf_options* destination) {
    
    HandleScope scope;
    
    // font directory
    if (source->Has(String::New("fontdirectory"))) {
        
        // get the provided path
        String::Utf8Value fontdirectory (source->Get(String::New("fontdirectory"))->ToString());
        
        // init destination string size
        destination->fontdirectory = new char[fontdirectory.length() + 1];
        
        // copy in provded path
        strncpy((char*)memset(destination->fontdirectory, '\0', fontdirectory.length() + 1), *fontdirectory, fontdirectory.length());
        
    } else {
        
        // init default path
        char fontdirectory[] = "/usr/share/fonts/truetype/msttcorefonts";
        
        // init destination string size
        destination->fontdirectory = new char[strlen(fontdirectory) + 1];
        
        // copy in default path
        strncpy((char*)memset(destination->fontdirectory, '\0', strlen(fontdirectory) + 1), fontdirectory, strlen(fontdirectory));
        
    }
    
    // watermark
    if (source->Has(String::New("watermark"))) {
        
        // get the provided path
        String::Utf8Value watermark (source->Get(String::New("watermark"))->ToString());
        
        // init destination string size
        destination->watermark = new char[watermark.length() + 1];
        
        // copy in provided path
        strncpy((char*)memset(destination->watermark, '\0', watermark.length() + 1), *watermark, watermark.length());
        
    }
    
}

void setOptions (VTHDOC documentHandle, topdf_options* options) {
    
    // set font directory
    DASetOption(documentHandle, SCCOPT_FONTDIRECTORY, options->fontdirectory, strlen(options->fontdirectory));
    
    // set watermark settings
    if (options->watermark != NULL) {
        
        WATERMARKIO watermarkSettings;
        WATERMARKPATH watermarkPathSettings;
        
        // set path settings
        watermarkPathSettings.dwMaxSize = SCCUT_FILENAMEMAX;
        strncpy((char*)memset(watermarkPathSettings.szWaterMarkPath, '\0', SCCUT_FILENAMEMAX), options->watermark, strlen(options->watermark));
        
        // set watermark settings
        watermarkSettings.dwType = IOTYPE_UNIXPATH;
        watermarkSettings.Path = watermarkPathSettings;
        
        VTBOOL enabled = 1;
        
        DASetOption(documentHandle, SCCOPT_ENABLEWATERMARK, &enabled, sizeof(VTBOOL));
        DASetOption(documentHandle, SCCOPT_WATERMARKIO, &watermarkSettings, sizeof(WATERMARKIO));
        
    }
    
}

void topdf_convert (uv_work_t* req) {
    
    // init the baton pointer
    topdf_convert_baton* baton = (topdf_convert_baton*) req->data;
    
    VTDWORD loadOptions = OI_INIT_DEFAULT | OI_INIT_NOSAVEOPTIONS | OI_INIT_NOLOADOPTIONS;
        
    // initialize data access module, which performs some disk io
    if (DAInitEx(DATHREAD_INIT_PTHREADS, loadOptions) == DAERR_OK) {
        
        VTHDOC documentHandle;
        
        // open the source document
        if (DAOpenDocument(&documentHandle, IOTYPE_UNIXPATH, baton->source, 0) == SCCERR_OK) {
            
            VTHEXPORT exportHandle;
            
            // set options for the documents
            setOptions(documentHandle, baton->options);
            
            // convert the source document
            if (EXOpenExport(documentHandle, FI_PDF, IOTYPE_UNIXPATH, baton->destination, 0, 0, NULL, 0, &exportHandle) == SCCERR_OK) {
                
                if (EXRunExport(exportHandle) == SCCERR_OK) {
                    
                    baton->success = true;
                    
                }
                
                // close the export engine
                EXCloseExport(exportHandle);
                
            }
            
            // close the opened document
            DACloseDocument(documentHandle);
            
        }
        
        DADeInit();
        
    }
    
}

void topdf_convert_end (uv_work_t* req) {
    
    HandleScope scope;
    
    // init the baton pointer
    topdf_convert_baton* baton = (topdf_convert_baton*) req->data;
    
    // init callback params
    Local<Value> argv[2];
    
    if (baton->success == true) {
        
        argv[0] = Local<Value>::New(Null());
        argv[1] = Local<Value>::New(Boolean::New(true));
        
    } else {
        
        argv[0] = Exception::Error(String::New("failed to convert file"));
        argv[1] = Local<Value>::New(Boolean::New(false));
        
    }
    
    // execute callback function
    node::MakeCallback(Context::GetCurrent()->Global(), baton->callback, 2, argv);
    
    delete baton;
    
}

Handle<Value> convert (const Arguments& args) {
    
    HandleScope scope;
    
    if (args.Length() != 4)
        ThrowException(Exception::SyntaxError(String::New("expected four parameters")));
    
    if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsObject() || !args[3]->IsFunction())
        ThrowException(Exception::TypeError(String::New("expected str, str, obj, func parameters")));
    
    String::Utf8Value source (args[0]->ToString());
    String::Utf8Value destination (args[1]->ToString());
    
    // instanciate data baton
    topdf_convert_baton* baton = new topdf_convert_baton;
    
    // set async work data thing
    baton->req.data = (void*) baton;
    
    // init string memory locations
    baton->source = new char[source.length() + 1];
    baton->destination = new char[destination.length() + 1];
    
    // set baton properties
    baton->success = false;;
    baton->options = new topdf_options;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[3]));
    
    // copy in string paths
    strncpy((char*)memset(baton->source, '\0', source.length() + 1), *source, source.length());
    strncpy((char*)memset(baton->destination, '\0', destination.length() + 1), *destination, destination.length());
    
    // set the desired options
    initializeOptions(args[2]->ToObject(), baton->options);
    
    // initiate async work on thread pool
    uv_queue_work(uv_default_loop(), &baton->req, topdf_convert, topdf_convert_end);
    
    return scope.Close(Undefined());
    
}

void initialize (Handle<Object> target) {
    
    target->Set(String::NewSymbol("convert"), FunctionTemplate::New(convert)->GetFunction());
    
}

NODE_MODULE(topdf, initialize)
