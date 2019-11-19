// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_to_java_messaging.h"

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser_jni_headers/WebMessageListenerHolder_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/stl_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

namespace android_webview {

JsToJavaMessaging::JsToJavaMessaging(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::JsToJavaMessaging> receiver,
    base::android::ScopedJavaGlobalRef<jobject> listener_ref,
    const net::ProxyBypassRules& allowed_origin_rules)
    : render_frame_host_(render_frame_host),
      listener_ref_(listener_ref),
      allowed_origin_rules_(allowed_origin_rules) {
  receiver_.Bind(std::move(receiver));
}

JsToJavaMessaging::~JsToJavaMessaging() {}

void JsToJavaMessaging::PostMessage(
    const base::string16& message,
    std::vector<mojo::ScopedMessagePipeHandle> ports) {
  DCHECK(render_frame_host_);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);

  if (!web_contents)
    return;

  // |source_origin| has no race with this PostMessage call, because of
  // associated mojo channel, the committed origin message and PostMessage are
  // in sequence.
  url::Origin source_origin = render_frame_host_->GetLastCommittedOrigin();

  if (!allowed_origin_rules_.Matches(source_origin.GetURL()))
    return;

  std::vector<int> int_ports(ports.size(), MOJO_HANDLE_INVALID /* 0 */);
  for (size_t i = 0; i < ports.size(); ++i) {
    int_ports[i] = ports[i].release().value();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebMessageListenerHolder_onPostMessage(
      env, listener_ref_, base::android::ConvertUTF16ToJavaString(env, message),
      base::android::ConvertUTF8ToJavaString(env, source_origin.Serialize()),
      web_contents->GetMainFrame() == render_frame_host_,
      base::android::ToJavaIntArray(env, int_ports.data(), int_ports.size()),
      reply_proxy_->GetJavaPeer());
}

void JsToJavaMessaging::SetJavaToJsMessaging(
    mojo::PendingAssociatedRemote<mojom::JavaToJsMessaging>
        java_to_js_messaging) {
  // A RenderFrame may inject JsToJavaMessaging in the JavaScript context more
  // than once because of reusing of RenderFrame.
  reply_proxy_ =
      std::make_unique<JsReplyProxy>(std::move(java_to_js_messaging));
}

}  // namespace android_webview
