// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_REQUEST_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_REQUEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace net {
class HttpRequestHeaders;
}

namespace network {
struct ResourceRequest;
}

namespace android_webview {

// A passive data structure only used to carry request information. This
// class should be copyable.
// The fields are ultimately guided by android.webkit.WebResourceRequest:
// https://developer.android.com/reference/android/webkit/WebResourceRequest.html
struct AwWebResourceRequest final {
  explicit AwWebResourceRequest(const network::ResourceRequest& request);
  AwWebResourceRequest(const std::string& in_url,
                       const std::string& in_method,
                       bool in_is_outermost_main_frame,
                       bool in_has_user_gesture,
                       const net::HttpRequestHeaders& in_headers);

  // Add default copy/move/assign operators. Adding explicit destructor
  // prevents generating move operator.
  AwWebResourceRequest(const AwWebResourceRequest& other);
  AwWebResourceRequest(AwWebResourceRequest&& other);
  AwWebResourceRequest& operator=(AwWebResourceRequest&& other);
  ~AwWebResourceRequest();

  // The java equivalent
  struct AwJavaWebResourceRequest {
    AwJavaWebResourceRequest();
    ~AwJavaWebResourceRequest();

    base::android::ScopedJavaLocalRef<jstring> jurl;
    base::android::ScopedJavaLocalRef<jstring> jmethod;
    base::android::ScopedJavaLocalRef<jobjectArray> jheader_names;
    base::android::ScopedJavaLocalRef<jobjectArray> jheader_values;
  };

  // Convenience method to convert AwWebResourceRequest to Java equivalent.
  static void ConvertToJava(JNIEnv* env,
                            const AwWebResourceRequest& request,
                            AwJavaWebResourceRequest* jRequest);

  std::string url;
  std::string method;
  bool is_outermost_main_frame;
  bool has_user_gesture;
  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  std::optional<bool> is_renderer_initiated;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_REQUEST_H_
