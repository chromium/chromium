// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserSettingsTestBridge_jni.h"

using base::android::JavaParamRef;

static void JNI_SupervisedUserSettingsTestBridge_SetFilteringBehavior(
    JNIEnv* env,
    Profile* profile,
    jint setting) {
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(setting));
}

static void JNI_SupervisedUserSettingsTestBridge_SetManualFilterForHost(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jstring>& host,
    jboolean allowlist) {
  std::string host_string(base::android::ConvertJavaStringToUTF8(env, host));
  supervised_user_test_util::SetManualFilterForHost(profile, host_string,
                                                    allowlist);
}

namespace {
class StaticUrlCheckerClient : public safe_search_api::URLCheckerClient {
 public:
  explicit StaticUrlCheckerClient(
      safe_search_api::ClientClassification response)
      : response_(response) {}
  void CheckURL(const GURL& url, ClientCheckCallback callback) override {
    std::move(callback).Run(url, response_);
  }

 private:
  safe_search_api::ClientClassification response_;
};
}  // namespace

static void
JNI_SupervisedUserSettingsTestBridge_SetKidsManagementResponseForTesting(  // IN-TEST
    JNIEnv* env,
    Profile* profile,
    jboolean is_allowed) {
  SupervisedUserServiceFactory::GetInstance()
      ->GetForProfile(profile)
      ->GetURLFilter()
      ->SetURLCheckerClientForTesting(std::make_unique<StaticUrlCheckerClient>(
          is_allowed ? safe_search_api::ClientClassification::kAllowed
                     : safe_search_api::ClientClassification::kRestricted));
}

DEFINE_JNI(SupervisedUserSettingsTestBridge)
