// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/feed/android/feed_service_factory.h"
#include "chrome/browser/feed/android/jni_headers/FeedProcessScopeDependencyProvider_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/feed_stream_surface.h"
#include "components/variations/variations_ids_provider.h"

namespace feed {
namespace android {

static FeedApi* GetFeedApi() {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  if (!service)
    return nullptr;
  return service->GetStream();
}

static void JNI_FeedProcessScopeDependencyProvider_ProcessViewAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& data) {
  FeedApi* feed_stream_api = GetFeedApi();
  if (!feed_stream_api)
    return;
  std::string data_string;
  base::android::JavaByteArrayToString(env, data, &data_string);
  feed_stream_api->ProcessViewAction(data_string);
}

static void
JNI_FeedProcessScopeDependencyProvider_ProcessViewActionWithLoggingParameters(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& action_data,
    const base::android::JavaParamRef<jbyteArray>& logging_parameters) {
  FeedApi* feed_stream_api = GetFeedApi();
  if (!feed_stream_api)
    return;
  std::string action_data_string;
  base::android::JavaByteArrayToString(env, action_data, &action_data_string);
  std::string logging_parameters_string;
  base::android::JavaByteArrayToString(env, logging_parameters,
                                       &logging_parameters_string);
  feedui::LoggingParameters logging_parameters_value;
  if (!logging_parameters_value.ParseFromString(logging_parameters_string)) {
    DLOG(ERROR) << "Error parsing logging parameters";
    return;
  }
  feed_stream_api->ProcessViewAction(action_data_string,
                                     logging_parameters_value);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_FeedProcessScopeDependencyProvider_GetSessionId(JNIEnv* env) {
  std::string session;
  FeedApi* feed_stream_api = GetFeedApi();
  if (feed_stream_api) {
    session = feed_stream_api->GetSessionId();
  }

  return base::android::ConvertUTF8ToJavaString(env, session);
}

static base::android::ScopedJavaLocalRef<jintArray>
JNI_FeedProcessScopeDependencyProvider_GetExperimentIds(JNIEnv* env) {
  auto* variations_ids_provider =
      variations::VariationsIdsProvider::GetInstance();
  DCHECK(variations_ids_provider != nullptr);

  return base::android::ToJavaIntArray(
      env, variations_ids_provider->GetVariationsVectorForWebPropertiesKeys());
}

}  // namespace android
}  // namespace feed
