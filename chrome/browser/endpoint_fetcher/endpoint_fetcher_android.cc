// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/endpoint_fetcher/endpoint_fetcher.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/endpoint_fetcher/jni_headers/EndpointFetcher_jni.h"
#include "chrome/browser/endpoint_fetcher/jni_headers/EndpointResponse_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/storage_partition.h"

namespace {
static void OnEndpointFetcherComplete(
    const base::android::JavaRef<jobject>& jcaller,
    // Passing the endpoint_fetcher ensures the endpoint_fetcher's
    // lifetime extends to the callback and is not destroyed
    // prematurely (which would result in cancellation of the request).
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> endpoint_response) {
  base::android::RunObjectCallbackAndroid(
      jcaller, Java_EndpointResponse_createEndpointResponse(
                   base::android::AttachCurrentThread(),
                   base::android::ConvertUTF8ToJavaString(
                       base::android::AttachCurrentThread(),
                       std::move(endpoint_response->response))));
}
}  // namespace

// TODO(crbug.com/1077537) Create a KeyProvider so
// we can have one centralized API.

static void JNI_EndpointFetcher_NativeFetchOAuth(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jstring>& joauth_consumer_name,
    const base::android::JavaParamRef<jstring>& jurl,
    const base::android::JavaParamRef<jstring>& jhttps_method,
    const base::android::JavaParamRef<jstring>& jcontent_type,
    const base::android::JavaParamRef<jobjectArray>& jscopes,
    const base::android::JavaParamRef<jstring>& jpost_data,
    jlong jtimeout,
    jint jannotation_hash_code,
    const base::android::JavaParamRef<jobject>& jcallback) {
  std::vector<std::string> scopes;
  base::android::AppendJavaStringArrayToStringVector(env, jscopes, &scopes);
  auto endpoint_fetcher = std::make_unique<EndpointFetcher>(
      ProfileAndroid::FromProfileAndroid(jprofile)
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::android::ConvertJavaStringToUTF8(env, joauth_consumer_name),
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)),
      base::android::ConvertJavaStringToUTF8(env, jhttps_method),
      base::android::ConvertJavaStringToUTF8(env, jcontent_type), scopes,
      jtimeout, base::android::ConvertJavaStringToUTF8(env, jpost_data),
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          jannotation_hash_code),
      IdentityManagerFactory::GetForProfile(
          ProfileAndroid::FromProfileAndroid(jprofile)));
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->Fetch(
      base::BindOnce(&OnEndpointFetcherComplete,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback),
                     // unique_ptr endpoint_fetcher is passed until the callback
                     // to ensure its lifetime across the request.
                     std::move(endpoint_fetcher)));
}

static void JNI_EndpointFetcher_NativeFetchChromeAPIKey(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jstring>& jurl,
    const base::android::JavaParamRef<jstring>& jhttps_method,
    const base::android::JavaParamRef<jstring>& jcontent_type,
    const base::android::JavaParamRef<jstring>& jpost_data,
    jlong jtimeout,
    const base::android::JavaParamRef<jobjectArray>& jheaders,
    jint jannotation_hash_code,
    const base::android::JavaParamRef<jobject>& jcallback) {
  std::vector<std::string> headers;
  base::android::AppendJavaStringArrayToStringVector(env, jheaders, &headers);
  auto endpoint_fetcher = std::make_unique<EndpointFetcher>(
      ProfileAndroid::FromProfileAndroid(jprofile)
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)),
      base::android::ConvertJavaStringToUTF8(env, jhttps_method),
      base::android::ConvertJavaStringToUTF8(env, jcontent_type), jtimeout,
      base::android::ConvertJavaStringToUTF8(env, jpost_data), headers,
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          jannotation_hash_code),
      chrome::GetChannel() == version_info::Channel::STABLE);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&OnEndpointFetcherComplete,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback),
                     // unique_ptr endpoint_fetcher is passed until the callback
                     // to ensure its lifetime across the request.
                     std::move(endpoint_fetcher)),
      nullptr);
}

static void JNI_EndpointFetcher_NativeFetchWithNoAuth(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jstring>& jurl,
    jint jannotation_hash_code,
    const base::android::JavaParamRef<jobject>& jcallback) {
  auto endpoint_fetcher = std::make_unique<EndpointFetcher>(
      ProfileAndroid::FromProfileAndroid(jprofile)
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)),
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          jannotation_hash_code));
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&OnEndpointFetcherComplete,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback),
                     // unique_ptr endpoint_fetcher is passed until the callback
                     // to ensure its lifetime across the request.
                     std::move(endpoint_fetcher)),
      nullptr);
}
