// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/FamilyLinkSettingsTestBridge_jni.h"

namespace supervised_user {
using base::android::JavaRef;

static void JNI_FamilyLinkSettingsTestBridge_SetFilteringBehavior(
    JNIEnv* env,
    Profile* profile,
    int32_t setting) {
  FamilyLinkSettingsService* service =
      FamilyLinkSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  service->SetLocalSetting(kContentPackDefaultFilteringBehavior,
                           base::Value(setting));
}

static void JNI_FamilyLinkSettingsTestBridge_SetManualFilterForHost(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jstring>& host,
    bool allowlist) {
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
JNI_FamilyLinkSettingsTestBridge_SetKidsManagementResponseForTesting(  // IN-TEST
    JNIEnv* env,
    Profile* profile,
    bool is_allowed) {
  SupervisedUserServiceFactory::GetInstance()
      ->GetForProfile(profile)
      ->GetURLFilter()
      ->SetURLCheckerClientForTesting(std::make_unique<StaticUrlCheckerClient>(
          is_allowed ? safe_search_api::ClientClassification::kAllowed
                     : safe_search_api::ClientClassification::kRestricted));
}
}  // namespace supervised_user

DEFINE_JNI(FamilyLinkSettingsTestBridge)
