// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"

#include <utility>

#include "base/android/jni_string.h"
#include "components/js_injection/browser/web_message_reply_proxy.h"
#include "content/public/browser/android/message_payload.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/JsReplyProxy_jni.h"

namespace android_webview {

JsReplyProxy::JsReplyProxy(js_injection::WebMessageReplyProxy* reply_proxy)
    : reply_proxy_(reply_proxy) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_JsReplyProxy_create(env, reinterpret_cast<intptr_t>(this)));
}

JsReplyProxy::~JsReplyProxy() {
  if (!java_ref_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JsReplyProxy_onDestroy(env, java_ref_);
}

base::android::ScopedJavaLocalRef<jobject> JsReplyProxy::GetJavaPeer() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void JsReplyProxy::PostMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& payload) {
  reply_proxy_->PostWebMessage(
      content::android::ConvertToWebMessagePayloadFromJava(
          base::android::ScopedJavaLocalRef<jobject>(payload)));
}

}  // namespace android_webview
