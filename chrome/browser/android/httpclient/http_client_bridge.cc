// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/httpclient/http_client_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/httpclient/http_client.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/httpclient/jni_headers/SimpleHttpClient_jni.h"

using jni_zero::JavaParamRef;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;

namespace httpclient {

// static
jlong JNI_SimpleHttpClient_Init(JNIEnv* env, Profile* profile) {
  return reinterpret_cast<intptr_t>(new HttpClientBridge(profile));
}

HttpClientBridge::HttpClientBridge(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  http_client_ = std::make_unique<HttpClient>(profile->GetURLLoaderFactory());
}

HttpClientBridge::~HttpClientBridge() = default;

void HttpClientBridge::Destroy(JNIEnv* env) {
  delete this;
}

void HttpClientBridge::SendNetworkRequest(
    JNIEnv* env,
    GURL& gurl,
    std::string& request_type,
    std::vector<uint8_t>& request_body,
    std::map<std::string, std::string> headers,
    jint j_network_annotation_hashcode,
    const base::android::JavaParamRef<jobject>& j_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(gurl.is_valid());
  net::NetworkTrafficAnnotationTag tag =
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          j_network_annotation_hashcode);

  HttpClient::ResponseCallback callback = base::BindOnce(
      &HttpClientBridge::OnResult, weak_ptr_factory_.GetWeakPtr(),
      ScopedJavaGlobalRef<jobject>(env, j_callback));

  http_client_->Send(gurl, request_type, std::move(request_body),
                     std::move(headers), tag, std::move(callback));
}

void HttpClientBridge::OnResult(
    const ScopedJavaGlobalRef<jobject>& j_callback,
    int32_t http_code,
    int32_t net_error_code,
    std::vector<uint8_t>&& response_bytes,
    std::map<std::string, std::string>&& response_headers) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_http_response =
      Java_SimpleHttpClient_createHttpResponse(
          env, http_code, net_error_code, response_bytes, response_headers);
  base::android::RunObjectCallbackAndroid(j_callback, j_http_response);
}

}  // namespace httpclient
