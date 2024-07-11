// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/feed/android/jni_translation.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/variations/variations_ids_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedProcessScopeDependencyProvider_jni.h"

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
    const base::android::JavaParamRef<jbyteArray>& action_data,
    const base::android::JavaParamRef<jbyteArray>& logging_parameters) {
  FeedApi* feed_stream_api = GetFeedApi();
  if (!feed_stream_api)
    return;
  std::string action_data_string;
  base::android::JavaByteArrayToString(env, action_data, &action_data_string);

  feed_stream_api->ProcessViewAction(
      action_data_string, ToNativeLoggingParameters(env, logging_parameters));
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

  // Include the experiment IDs from Finch.
  std::vector<int> experiment_ids =
      variations_ids_provider->GetVariationsVectorForWebPropertiesKeys();

  // Include the synthetic experiment IDs sent by the server.
  FeedService* service = FeedServiceFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
  if (service) {
    const Experiments& experiments = service->GetExperiments();
    for (const auto& e : experiments) {
      for (const auto& g : e.second) {
        experiment_ids.push_back(g.experiment_id);
      }
    }
  }

  return base::android::ToJavaIntArray(env, experiment_ids);
}

}  // namespace android
}  // namespace feed
