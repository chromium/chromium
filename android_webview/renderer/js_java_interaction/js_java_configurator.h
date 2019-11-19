// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_
#define ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_

#include <vector>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"

namespace content {
class RenderFrame;
}

namespace android_webview {

class JsBinding;

class JsJavaConfigurator : public mojom::JsJavaConfigurator,
                           public content::RenderFrameObserver {
 public:
  explicit JsJavaConfigurator(content::RenderFrame* render_frame);
  ~JsJavaConfigurator() override;

  // mojom::Configurator implementation
  void SetJsObjects(std::vector<mojom::JsObjectPtr> js_object_ptrs) override;

  // RenderFrameObserver implementation
  void DidClearWindowObject() override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override;
  void OnDestruct() override;

  mojom::JsToJavaMessaging* GetJsToJavaMessage(
      const base::string16& js_object_name);

 private:
  struct JsObjectInfo;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::JsJavaConfigurator>
          pending_receiver);

  using JsObjectMap = std::map<base::string16, std::unique_ptr<JsObjectInfo>>;
  JsObjectMap js_objects_;

  // In some cases DidClearWindowObject will be called twice in a row, we need
  // to prevent doing multiple injection in that case.
  bool inside_did_clear_window_object_ = false;

  std::vector<std::unique_ptr<JsBinding>> js_bindings_;

  // Associated with legacy IPC channel.
  mojo::AssociatedReceiver<mojom::JsJavaConfigurator> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsJavaConfigurator);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_H_
