// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedImageFetchClient_jni.h"

using base::android::JavaParamRef;

namespace feed {
namespace {

void OnFetchFinished(JNIEnv* env,
                     base::android::ScopedJavaGlobalRef<jobject> callback,
                     NetworkResponse response) {
  Java_FeedImageFetchClient_onHttpResponse(
      env, callback, response.status_code,
      base::android::ToJavaByteArray(env, response.response_bytes));
}

FeedApi* GetFeedStream() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return nullptr;

  FeedService* feed_service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!feed_service)
    return nullptr;

  return feed_service->GetStream();
}

}  // namespace

jint JNI_FeedImageFetchClient_SendRequest(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_response_callback) {
  // Keep the callback as a ScopedJavaGlobalRef to enable binding it for use
  // with OnFetchFinished.
  base::android::ScopedJavaGlobalRef<jobject> callback(j_response_callback);

  FeedApi* stream = GetFeedStream();
  if (!stream) {
    OnFetchFinished(env, std::move(callback), {});
    return 0;
  }

  return stream
      ->FetchImage(GURL(base::android::ConvertJavaStringToUTF8(env, j_url)),
                   base::BindOnce(&OnFetchFinished, env, std::move(callback)))
      .GetUnsafeValue();
}

void JNI_FeedImageFetchClient_Cancel(JNIEnv* env, jint j_request_id) {
  FeedApi* stream = GetFeedStream();
  if (!stream)
    return;

  stream->CancelImageFetch(ImageFetchId::FromUnsafeValue(j_request_id));
}

}  // namespace feed
