// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/httpclient/http_client_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/httpclient/http_client.h"
#include "chrome/browser/android/httpclient/jni_headers/SimpleHttpClient_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace httpclient {

// static
jlong JNI_SimpleHttpClient_Init(JNIEnv* env,
                                const JavaParamRef<jobject>& j_profile) {
  return reinterpret_cast<intptr_t>(new HttpClientBridge(j_profile));
}

HttpClientBridge::HttpClientBridge(const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  http_client_ = std::make_unique<HttpClient>(profile->GetURLLoaderFactory());
}

HttpClientBridge::~HttpClientBridge() = default;

void HttpClientBridge::Destroy(JNIEnv* env) {
  delete this;
}

void HttpClientBridge::SendNetworkRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl,
    const base::android::JavaParamRef<jstring>& j_request_type,
    const base::android::JavaParamRef<jbyteArray>& j_body,
    const base::android::JavaParamRef<jobjectArray>& j_header_keys,
    const base::android::JavaParamRef<jobjectArray>& j_header_values,
    jint j_network_annotation_hashcode,
    const base::android::JavaParamRef<jobject>& j_callback) {
  DCHECK(j_gurl);
  std::unique_ptr<GURL> gurl = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  DCHECK(gurl->is_valid());
  std::vector<uint8_t> request_body;
  base::android::JavaByteArrayToByteVector(env, j_body, &request_body);
  std::vector<std::string> header_keys;
  base::android::AppendJavaStringArrayToStringVector(env, j_header_keys,
                                                     &header_keys);
  std::vector<std::string> header_values;
  base::android::AppendJavaStringArrayToStringVector(env, j_header_values,
                                                     &header_values);
  net::NetworkTrafficAnnotationTag tag =
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          j_network_annotation_hashcode);
  // base::Unretained is safe because we are an immortal singleton.
  HttpClient::ResponseCallback callback =
      base::BindOnce(&HttpClientBridge::OnResult, base::Unretained(this),
                     ScopedJavaGlobalRef<jobject>(env, j_callback));

  // base::Unretained is safe because we are an immortal singleton.
  // Post on the UI thread because HttpClient needs to be called on the UI
  // thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&HttpClient::Send, base::Unretained(http_client_.get()),
                     *gurl, ConvertJavaStringToUTF8(env, j_request_type),
                     std::move(request_body), std::move(header_keys),
                     std::move(header_values), tag, std::move(callback)));
}

void HttpClientBridge::OnResult(
    const ScopedJavaGlobalRef<jobject>& j_callback,
    int32_t http_code,
    int32_t net_error_code,
    std::vector<uint8_t>&& response_bytes,
    std::vector<std::string>&& response_header_keys,
    std::vector<std::string>&& response_header_values) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response_bytes =
      base::android::ToJavaByteArray(env, response_bytes);
  ScopedJavaLocalRef<jobjectArray> j_response_header_keys =
      base::android::ToJavaArrayOfStrings(env, response_header_keys);
  ScopedJavaLocalRef<jobjectArray> j_response_header_values =
      base::android::ToJavaArrayOfStrings(env, response_header_values);
  ScopedJavaLocalRef<jobject> j_http_response =
      Java_SimpleHttpClient_createHttpResponse(
          env, http_code, net_error_code, j_response_bytes,
          j_response_header_keys, j_response_header_values);
  base::android::RunObjectCallbackAndroid(j_callback, j_http_response);
}

}  // namespace httpclient
