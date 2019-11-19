// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_TO_JAVA_MESSAGING_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_TO_JAVA_MESSAGING_H_

#include <vector>

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"
#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"

namespace content {
class RenderFrameHost;
}

namespace android_webview {

// Implementation of mojo::JsToJavaMessaging interface. Receives PostMessage()
// call from renderer JsBinding.
class JsToJavaMessaging : public mojom::JsToJavaMessaging {
 public:
  JsToJavaMessaging(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<mojom::JsToJavaMessaging> receiver,
      base::android::ScopedJavaGlobalRef<jobject> listener_ref,
      const net::ProxyBypassRules& allowed_origin_rules);
  ~JsToJavaMessaging() override;

  // mojom::JsToJavaMessaging implementation.
  void PostMessage(const base::string16& message,
                   std::vector<mojo::ScopedMessagePipeHandle> ports) override;
  void SetJavaToJsMessaging(
      mojo::PendingAssociatedRemote<mojom::JavaToJsMessaging>
          java_to_js_messaging) override;

 private:
  content::RenderFrameHost* render_frame_host_;
  std::unique_ptr<JsReplyProxy> reply_proxy_;
  base::android::ScopedJavaGlobalRef<jobject> listener_ref_;
  net::ProxyBypassRules allowed_origin_rules_;
  mojo::AssociatedReceiver<mojom::JsToJavaMessaging> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsToJavaMessaging);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_TO_JAVA_MESSAGING_H_
