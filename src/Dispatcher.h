/*
 * FixAcceptor.h
 *
 *  Created on: Mar 20, 2015
 *      Author: skutko
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include <v8.h>
#include <node.h>
#include <nan.h>
#include "FixEvent.h"
#include "FixMessageUtil.h"
#include "FixLoginResponse.h"
#include "Threading.h"

using namespace v8;
using namespace node;

class Dispatcher {
  public:
    static Dispatcher& getInstance() {
        static Dispatcher instance;
        return instance;
    }

    static void listener(uv_async_t* handle, int status) {
        NanScope();

        //std::cout << "received handleFixEvent " << '\n';

        Dispatcher* dispatcher = static_cast<Dispatcher*>(handle->data);
        std::vector<fix_event_t*> events;

        NODE_QUICKFIX_MUTEX_LOCK(&dispatcher->mutex);
        events.swap(dispatcher->data);
        NODE_QUICKFIX_MUTEX_UNLOCK(&dispatcher->mutex);

        for (unsigned int i = 0, size = events.size(); i < size; i++) {
            fix_event_t* event = events[i];

            Local<String> eventName = NanNew<String>(event->eventName.c_str());
            Local<Function> callback = Local<Function>::Cast((*event->callbacks)->Get(eventName));

            std::vector< Local<Value> > arguments;
            if(event->logon != NULL) {
                callback = event->logon->GetFunction();
                Handle<Object> jsLogonResponse = FixLoginResponse::wrapFixLoginResponse(event->logonResponse);
                arguments.push_back(NanNew<Object>(jsLogonResponse));
            }

            if(event->message != NULL) {
                Local<Object> msg = NanNew<Object>();
                FixMessageUtil::fix2Js(msg, event->message);
                arguments.push_back(msg);
                arguments.push_back(FixMessageUtil::sessionIdToJs(event->sessionId));
                
            } else {
                arguments.push_back(FixMessageUtil::sessionIdToJs(event->sessionId));
            }

            //std::cout << "handleFixEvent " << event->eventName << '\n' << std::flush;
            NanMakeCallback(NanGetCurrentContext()->Global(), callback, arguments.size(), &arguments[0]);
        }
    }

    void dispatch(fix_event_t* event) {
        //std::cout << "dispatching onCreate event " << '\n';

        NODE_QUICKFIX_MUTEX_LOCK(&mutex);
        data.push_back(event);
        NODE_QUICKFIX_MUTEX_UNLOCK(&mutex);

        uv_async_send(&watcher);
    }

  private:
    uv_async_t watcher;
    NODE_QUICKFIX_MUTEX_t
    std::vector<fix_event_t*> data;

    Dispatcher() {
        watcher.data = this;
        NODE_QUICKFIX_MUTEX_INIT
        uv_async_init(uv_default_loop(), &watcher, reinterpret_cast<uv_async_cb>(listener));
    };

    ~Dispatcher() {
        NODE_QUICKFIX_MUTEX_DESTROY
    }

    Dispatcher(Dispatcher const&)      = delete;
    void operator=(Dispatcher const&)  = delete;
};

#endif /* DISPATCHER_H_ */
