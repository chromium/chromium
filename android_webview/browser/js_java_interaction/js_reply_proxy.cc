// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"

#include "android_webview/browser_jni_headers/JsReplyProxy_jni.h"
#include "base/android/jni_string.h"

namespace android_webview {

JsReplyProxy::JsReplyProxy(
    mojo::PendingAssociatedRemote<mojom::JavaToJsMessaging>
        java_to_js_messaging)
    : java_to_js_messaging_(std::move(java_to_js_messaging)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_JsReplyProxy_create(env, reinterpret_cast<intptr_t>(this)));
}

JsReplyProxy::~JsReplyProxy() {
  if (java_ref_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JsReplyProxy_onDestroy(env, java_ref_);
}

base::android::ScopedJavaLocalRef<jobject> JsReplyProxy::GetJavaPeer() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void JsReplyProxy::PostMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& message) {
  DCHECK(java_to_js_messaging_);
  java_to_js_messaging_->OnPostMessage(
      base::android::ConvertJavaStringToUTF16(env, message));
}

}  // namespace android_webview
