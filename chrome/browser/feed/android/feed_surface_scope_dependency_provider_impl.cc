// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/feed/android/jni_translation.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/variations/variations_ids_provider.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedSurfaceScopeDependencyProviderImpl_jni.h"

namespace feed::android {

namespace {

static FeedApi* GetFeedApi() {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  if (!service) {
    return nullptr;
  }
  return service->GetStream();
}

base::android::ScopedJavaLocalRef<jobject> ToJava(
    JNIEnv* env,
    const NetworkResponse& response) {
  return Java_NetworkResponse_Constructor(
      env, response.status_code == 200, response.status_code,
      base::android::ToJavaArrayOfStrings(
          env, response.response_header_names_and_values),
      base::android::ToJavaByteArray(env, response.response_bytes));
}

void OnFetchResourceFinished(JNIEnv* env,
                             const base::android::JavaRef<jobject>& callback,
                             NetworkResponse response) {
  base::android::RunObjectCallbackAndroid(callback, ToJava(env, response));
}

}  // namespace

static void JNI_FeedSurfaceScopeDependencyProviderImpl_FetchResource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jstring>& j_method,
    const base::android::JavaParamRef<jobjectArray>& j_header_name_and_values,
    const base::android::JavaParamRef<jbyteArray>& j_post_data,
    const base::android::JavaParamRef<jobject>& callback_obj) {
  FeedApi* feed_stream_api = GetFeedApi();
  if (!feed_stream_api) {
    return;
  }
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::vector<std::string> header_name_and_values;
  base::android::AppendJavaStringArrayToStringVector(
      env, j_header_name_and_values, &header_name_and_values);
  std::string post_data;
  base::android::JavaByteArrayToString(env, j_post_data, &post_data);
  feed_stream_api->FetchResource(
      url, base::android::ConvertJavaStringToUTF8(env, j_method),
      header_name_and_values, post_data,
      base::BindOnce(
          &OnFetchResourceFinished, env,
          base::android::ScopedJavaGlobalRef<jobject>(callback_obj)));
}

}  // namespace feed::android
