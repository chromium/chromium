// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace android_webview {

class JsReplyProxy {
 public:
  explicit JsReplyProxy(mojo::PendingAssociatedRemote<mojom::JavaToJsMessaging>
                            java_to_js_messaging);

  ~JsReplyProxy();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();

  void PostMessage(JNIEnv* env,
                   const base::android::JavaParamRef<jstring>& message);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  mojo::AssociatedRemote<mojom::JavaToJsMessaging> java_to_js_messaging_;

  DISALLOW_COPY_AND_ASSIGN(JsReplyProxy);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
