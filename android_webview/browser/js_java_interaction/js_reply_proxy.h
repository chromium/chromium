// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace js_injection {
class WebMessageReplyProxy;
}

namespace android_webview {

class JsReplyProxy {
 public:
  explicit JsReplyProxy(js_injection::WebMessageReplyProxy* reply_proxy);

  JsReplyProxy(const JsReplyProxy&) = delete;
  JsReplyProxy& operator=(const JsReplyProxy&) = delete;

  ~JsReplyProxy();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();

  void PostMessage(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& payload);

 private:
  raw_ptr<js_injection::WebMessageReplyProxy> reply_proxy_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_REPLY_PROXY_H_
