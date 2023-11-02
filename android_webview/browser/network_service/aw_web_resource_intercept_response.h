// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_INTERCEPT_RESPONSE_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_INTERCEPT_RESPONSE_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"

namespace embedder_support {
class WebResourceResponse;
}

namespace android_webview {

class AwWebResourceInterceptResponse {
 public:
  AwWebResourceInterceptResponse() = delete;

  // It is expected that |obj| is an instance of the Java-side
  // org.chromium.android_webview.AwWebResourceInterceptResponse class.
  explicit AwWebResourceInterceptResponse(
      const base::android::JavaRef<jobject>& obj);

  AwWebResourceInterceptResponse(const AwWebResourceInterceptResponse&) =
      delete;
  AwWebResourceInterceptResponse& operator=(
      const AwWebResourceInterceptResponse&) = delete;

  ~AwWebResourceInterceptResponse();

  // True if the call to shouldInterceptRequest raised an exception.
  bool RaisedException(JNIEnv* env) const;

  // True if this object contains a response.
  bool HasResponse(JNIEnv* env) const;

  // The response returned by the Java-side handler. Caller should first check
  // if an exception was caught via RaisedException() before calling
  // this method. A null value means do not intercept the response.
  std::unique_ptr<embedder_support::WebResourceResponse> GetResponse(
      JNIEnv* env) const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_INTERCEPT_RESPONSE_H_
