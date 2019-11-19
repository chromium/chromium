// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_RESPONSE_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_RESPONSE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace net {
class HttpResponseHeaders;
}

namespace android_webview {

class InputStream;

// This class represents the Java-side data that is to be used to complete a
// particular URLRequest.
class AwWebResourceResponse {
 public:
  // It is expected that |obj| is an instance of the Java-side
  // org.chromium.android_webview.AwWebResourceResponse class.
  explicit AwWebResourceResponse(const base::android::JavaRef<jobject>& obj);
  ~AwWebResourceResponse();

  bool HasInputStream(JNIEnv* env) const;
  std::unique_ptr<InputStream> GetInputStream(JNIEnv* env);
  bool GetMimeType(JNIEnv* env, std::string* mime_type) const;
  bool GetCharset(JNIEnv* env, std::string* charset) const;
  bool GetStatusInfo(JNIEnv* env,
                     int* status_code,
                     std::string* reason_phrase) const;
  // If true is returned then |headers| contain the headers, if false is
  // returned |headers| were not updated.
  bool GetResponseHeaders(JNIEnv* env, net::HttpResponseHeaders* headers) const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  bool input_stream_transferred_;

  DISALLOW_COPY_AND_ASSIGN(AwWebResourceResponse);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_WEB_RESOURCE_RESPONSE_H_
