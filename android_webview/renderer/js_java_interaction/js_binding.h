// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_
#define ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_

#include <string>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/auto_reset.h"
#include "base/strings/string16.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace v8 {
template <typename T>
class Global;
class Function;
}  // namespace v8

namespace content {
class RenderFrame;
}  // namespace content

namespace android_webview {
class JsJavaConfigurator;
// A gin::Wrappable class used for providing JavaScript API. We will inject the
// object of this class to JavaScript world in JsJavaConfigurator.
// JsJavaConfigurator will own at most one instance of this class. When the
// RenderFrame gone or another DidClearWindowObject comes, the instance will be
// destroyed.
class JsBinding : public gin::Wrappable<JsBinding>,
                  public mojom::JavaToJsMessaging {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static std::unique_ptr<JsBinding> Install(
      content::RenderFrame* render_frame,
      const base::string16& js_object_name,
      JsJavaConfigurator* js_java_configurator);

  // mojom::JavaToJsMessaging implementation.
  void OnPostMessage(const base::string16& message) override;

  void ReleaseV8GlobalObjects();

  ~JsBinding() final;

 private:
  explicit JsBinding(content::RenderFrame* render_frame,
                     const base::string16& js_object_name,
                     JsJavaConfigurator* js_java_configurator);

  // gin::Wrappable implementation
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // For jsObject.postMessage(message[, ports]) JavaScript API.
  void PostMessage(gin::Arguments* args);
  // For jsObject.addEventListener("message", listener) JavaScript API.
  void AddEventListener(gin::Arguments* args);
  // For jsObject.removeEventListener("message", listener) JavaScript API.
  void RemoveEventListener(gin::Arguments* args);
  // For get jsObject.onmessage.
  v8::Local<v8::Function> GetOnMessage(v8::Isolate* isolate);
  // For set jsObject.onmessage.
  void SetOnMessage(v8::Isolate* isolate, v8::Local<v8::Value> value);

  content::RenderFrame* render_frame_;
  base::string16 js_object_name_;
  v8::Global<v8::Function> on_message_;
  std::vector<v8::Global<v8::Function>> listeners_;
  // |js_java_configurator| owns JsBinding objects, so it will out live
  // JsBinding's life cycle, it is safe to access it.
  JsJavaConfigurator* js_java_configurator_;

  mojo::AssociatedReceiver<mojom::JavaToJsMessaging> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsBinding);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_
