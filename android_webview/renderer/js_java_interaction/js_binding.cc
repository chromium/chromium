// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/js_java_interaction/js_binding.h"

#include <vector>

#include "android_webview/renderer/js_java_interaction/js_java_configurator.h"
#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_message_port_converter.h"
#include "v8/include/v8.h"

namespace {
constexpr char kPostMessage[] = "postMessage";
constexpr char kOnMessage[] = "onmessage";
constexpr char kAddEventListener[] = "addEventListener";
constexpr char kRemoveEventListener[] = "removeEventListener";
}  // anonymous namespace

namespace android_webview {

gin::WrapperInfo JsBinding::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
std::unique_ptr<JsBinding> JsBinding::Install(
    content::RenderFrame* render_frame,
    const base::string16& js_object_name,
    JsJavaConfigurator* js_java_configurator) {
  CHECK(!js_object_name.empty())
      << "JavaScript wrapper name shouldn't be empty";

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;

  v8::Context::Scope context_scope(context);
  std::unique_ptr<JsBinding> js_binding(
      new JsBinding(render_frame, js_object_name, js_java_configurator));
  gin::Handle<JsBinding> bindings =
      gin::CreateHandle(isolate, js_binding.get());
  if (bindings.IsEmpty())
    return nullptr;

  v8::Local<v8::Object> global = context->Global();
  global
      ->CreateDataProperty(context,
                           gin::StringToSymbol(isolate, js_object_name),
                           bindings.ToV8())
      .Check();

  return js_binding;
}

JsBinding::JsBinding(content::RenderFrame* render_frame,
                     const base::string16& js_object_name,
                     JsJavaConfigurator* js_java_configurator)
    : render_frame_(render_frame),
      js_object_name_(js_object_name),
      js_java_configurator_(js_java_configurator) {
  mojom::JsToJavaMessaging* js_to_java_messaging =
      js_java_configurator_->GetJsToJavaMessage(js_object_name_);
  if (js_to_java_messaging) {
    js_to_java_messaging->SetJavaToJsMessaging(
        receiver_.BindNewEndpointAndPassRemote());
  }
}

JsBinding::~JsBinding() = default;

void JsBinding::OnPostMessage(const base::string16& message) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (!web_frame)
    return;

  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  // Setting verbose makes the exception get reported to the default
  // uncaught-exception handlers, rather than just being silently swallowed.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  // Simulate MessageEvent's data property. See
  // https://html.spec.whatwg.org/multipage/comms.html#messageevent
  v8::Local<v8::Object> event =
      gin::DataObjectBuilder(isolate).Set("data", message).Build();
  v8::Local<v8::Value> argv[] = {event};

  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Function> on_message = GetOnMessage(isolate);
  if (!on_message.IsEmpty()) {
    web_frame->RequestExecuteV8Function(context, on_message, self, 1, argv,
                                        nullptr);
  }

  for (const auto& listener : listeners_) {
    web_frame->RequestExecuteV8Function(context, listener.Get(isolate), self, 1,
                                        argv, nullptr);
  }
}

void JsBinding::ReleaseV8GlobalObjects() {
  listeners_.clear();
  on_message_.Reset();
}

gin::ObjectTemplateBuilder JsBinding::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<JsBinding>::GetObjectTemplateBuilder(isolate)
      .SetMethod(kPostMessage, &JsBinding::PostMessage)
      .SetMethod(kAddEventListener, &JsBinding::AddEventListener)
      .SetMethod(kRemoveEventListener, &JsBinding::RemoveEventListener)
      .SetProperty(kOnMessage, &JsBinding::GetOnMessage,
                   &JsBinding::SetOnMessage);
}

void JsBinding::PostMessage(gin::Arguments* args) {
  base::string16 message;
  if (!args->GetNext(&message)) {
    args->ThrowError();
    return;
  }

  std::vector<blink::MessagePortChannel> ports;
  std::vector<v8::Local<v8::Object>> objs;
  // If we get more than two arguments and the second argument is not an array
  // of ports, we can't process.
  if (args->Length() >= 2 && !args->GetNext(&objs)) {
    args->ThrowError();
    return;
  }

  for (auto& obj : objs) {
    base::Optional<blink::MessagePortChannel> port =
        blink::WebMessagePortConverter::DisentangleAndExtractMessagePortChannel(
            args->isolate(), obj);
    // If the port is null we should throw an exception.
    if (!port.has_value()) {
      args->ThrowError();
      return;
    }
    ports.emplace_back(port.value());
  }

  mojom::JsToJavaMessaging* js_to_java_messaging =
      js_java_configurator_->GetJsToJavaMessage(js_object_name_);
  if (js_to_java_messaging) {
    js_to_java_messaging->PostMessage(
        message, blink::MessagePortChannel::ReleaseHandles(ports));
  }
}

// AddEventListener() needs to match EventTarget's AddEventListener() in blink.
// It takes |type|, |listener| parameters, we ignore the |options| parameter.
// See https://dom.spec.whatwg.org/#dom-eventtarget-addeventlistener
void JsBinding::AddEventListener(gin::Arguments* args) {
  std::string type;
  if (!args->GetNext(&type)) {
    args->ThrowError();
    return;
  }

  // We only support message event.
  if (type != "message")
    return;

  v8::Local<v8::Function> listener;
  if (!args->GetNext(&listener))
    return;

  // Should be at most 3 parameters.
  if (args->Length() > 3) {
    args->ThrowError();
    return;
  }

  if (base::Contains(listeners_, listener))
    return;

  v8::Local<v8::Context> context = args->GetHolderCreationContext();
  listeners_.push_back(
      v8::Global<v8::Function>(context->GetIsolate(), listener));
}

// RemoveEventListener() needs to match EventTarget's RemoveEventListener() in
// blink. It takes |type|, |listener| parameters, we ignore |options| parameter.
// See https://dom.spec.whatwg.org/#dom-eventtarget-removeeventlistener
void JsBinding::RemoveEventListener(gin::Arguments* args) {
  std::string type;
  if (!args->GetNext(&type)) {
    args->ThrowError();
    return;
  }

  // We only support message event.
  if (type != "message")
    return;

  v8::Local<v8::Function> listener;
  if (!args->GetNext(&listener))
    return;

  // Should be at most 3 parameters.
  if (args->Length() > 3) {
    args->ThrowError();
    return;
  }

  auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
  if (iter == listeners_.end())
    return;

  listeners_.erase(iter);
}

v8::Local<v8::Function> JsBinding::GetOnMessage(v8::Isolate* isolate) {
  return on_message_.Get(isolate);
}

void JsBinding::SetOnMessage(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value->IsFunction())
    on_message_.Reset(isolate, value.As<v8::Function>());
  else
    on_message_.Reset();
}

}  // namespace android_webview
