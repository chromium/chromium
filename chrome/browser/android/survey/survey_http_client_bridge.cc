// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/survey/survey_http_client_bridge.h"
#include "chrome/browser/android/survey/survey_http_client.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SurveyHttpClientBridge_jni.h"
#include "chrome/browser/android/survey/http_client_type.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace survey {


// static
jlong JNI_SurveyHttpClientBridge_Init(JNIEnv* env,
                                      jint j_client_type,
                                      const JavaParamRef<jobject>& j_profile) {
  return reinterpret_cast<intptr_t>(
      new SurveyHttpClientBridge(j_client_type, j_profile));
}

SurveyHttpClientBridge::SurveyHttpClientBridge(
    jint j_client_type,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  DCHECK(profile);
  survey_http_client_ = std::make_unique<SurveyHttpClient>(
      static_cast<HttpClientType>(j_client_type),
      profile->GetURLLoaderFactory());
}

SurveyHttpClientBridge::~SurveyHttpClientBridge() = default;

void SurveyHttpClientBridge::Destroy(JNIEnv* env) {
  delete this;
}

void SurveyHttpClientBridge::SendNetworkRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl,
    const base::android::JavaParamRef<jstring>& j_request_type,
    const base::android::JavaParamRef<jbyteArray>& j_body,
    const base::android::JavaParamRef<jobjectArray>& j_header_keys,
    const base::android::JavaParamRef<jobjectArray>& j_header_values,
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
  SurveyHttpClient::ResponseCallback callback = base::BindOnce(
      &SurveyHttpClientBridge::OnResult, weak_ptr_factory_.GetWeakPtr(),
      ScopedJavaGlobalRef<jobject>(env, j_callback));

  survey_http_client_->Send(*gurl, ConvertJavaStringToUTF8(env, j_request_type),
                            std::move(request_body), std::move(header_keys),
                            std::move(header_values), std::move(callback));
}

void SurveyHttpClientBridge::OnResult(
    const ScopedJavaGlobalRef<jobject>& j_callback,
    int32_t http_code,
    int32_t net_error_code,
    std::vector<uint8_t> response_bytes,
    std::vector<std::string> response_header_keys,
    std::vector<std::string> response_header_values) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response_bytes =
      base::android::ToJavaByteArray(env, response_bytes);
  ScopedJavaLocalRef<jobjectArray> j_response_header_keys =
      base::android::ToJavaArrayOfStrings(env, response_header_keys);
  ScopedJavaLocalRef<jobjectArray> j_response_header_values =
      base::android::ToJavaArrayOfStrings(env, response_header_values);
  ScopedJavaLocalRef<jobject> j_http_response =
      Java_SurveyHttpClientBridge_createHttpResponse(
          env, http_code, net_error_code, j_response_bytes,
          j_response_header_keys, j_response_header_values);
  base::android::RunObjectCallbackAndroid(j_callback, j_http_response);
}

}  // namespace survey
