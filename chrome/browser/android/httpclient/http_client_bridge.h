// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"

namespace httpclient {

class HttpClient;

// Native counterpart for HttpClientBridge on java side.
class HttpClientBridge {
 public:
  explicit HttpClientBridge(
      const base::android::JavaParamRef<jobject>& j_profile);

  ~HttpClientBridge();

  HttpClientBridge(const HttpClientBridge& client) = delete;
  HttpClientBridge& operator=(const HttpClientBridge& client) = delete;

  void Destroy(JNIEnv* env);

  void SendNetworkRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_gurl,
      const base::android::JavaParamRef<jstring>& j_request_type,
      const base::android::JavaParamRef<jbyteArray>& j_body,
      const base::android::JavaParamRef<jobjectArray>& j_header_keys,
      const base::android::JavaParamRef<jobjectArray>& j_header_values,
      jint j_network_annotation_hashcode,
      const base::android::JavaParamRef<jobject>& j_callback);

 private:
  void OnResult(const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
                int32_t http_code,
                int32_t net_error_code,
                std::vector<uint8_t>&& response_bytes,
                std::vector<std::string>&& response_header_keys,
                std::vector<std::string>&& response_header_values);

  std::unique_ptr<HttpClient> http_client_;
  base::WeakPtrFactory<HttpClientBridge> weak_ptr_factory_{this};
};

}  // namespace httpclient

#endif  // CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_BRIDGE_H_
