// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserSettingsTestBridge_jni.h"

using base::android::JavaParamRef;

void JNI_SupervisedUserSettingsTestBridge_SetFilteringBehavior(JNIEnv* env,
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

void JNI_SupervisedUserSettingsTestBridge_SetManualFilterForHost(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jstring>& host,
    jboolean allowlist) {
  std::string host_string(base::android::ConvertJavaStringToUTF8(env, host));
  supervised_user_test_util::SetManualFilterForHost(profile, host_string,
                                                    allowlist);
}
class TestUrlLoaderFactoryHelper {
 public:
  static TestUrlLoaderFactoryHelper* SharedInstance() {
    return base::Singleton<TestUrlLoaderFactoryHelper>::get();
  }

  void SetUp() {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
  }

  void TearDown() { test_url_loader_factory_.reset(); }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return std::ref(shared_url_loader_factory_);
  }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

void JNI_SupervisedUserSettingsTestBridge_SetKidsManagementResponseForTesting(  // IN-TEST
    JNIEnv* env,
    Profile* profile,
    jboolean is_allowed) {
  kidsmanagement::ClassifyUrlResponse response;
  auto url_classification =
      is_allowed ? kidsmanagement::ClassifyUrlResponse::ALLOWED
                 : kidsmanagement::ClassifyUrlResponse::RESTRICTED;

  response.set_display_classification(url_classification);
  std::string classify_url_service_url =
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
      "me:classifyUrl?alt=proto";
  network::TestURLLoaderFactory* test_url_loader_factory_ =
      TestUrlLoaderFactoryHelper::SharedInstance()->test_url_loader_factory();
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      TestUrlLoaderFactoryHelper::SharedInstance()->shared_url_loader_factory();

  test_url_loader_factory_->AddResponse(classify_url_service_url,
                                        response.SerializeAsString());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client =
      std::make_unique<supervised_user::KidsChromeManagementURLCheckerClient>(
          identity_manager, shared_url_loader_factory, /*country=*/"",
          version_info::Channel::UNKNOWN);
  supervised_user_service->GetURLFilter()->SetURLCheckerClient(
      std::move(url_checker_client));
}

void JNI_SupervisedUserSettingsTestBridge_SetSafeSearchResponseForTesting(  // IN-TEST
    JNIEnv* env,
    Profile* profile,
    jboolean is_allowed) {
  TestUrlLoaderFactoryHelper::SharedInstance()
      ->test_url_loader_factory()
      ->AddResponse(
          "https://safesearch.googleapis.com/v1:classify",
          base::StringPrintf(R"json({"displayClassification": "%s"})json",
                             (is_allowed ? "allowed" : "restricted")));
}

void JNI_SupervisedUserSettingsTestBridge_SetUpTestUrlLoaderFactoryHelper(
    JNIEnv* env) {
  TestUrlLoaderFactoryHelper::SharedInstance()->SetUp();
}

void JNI_SupervisedUserSettingsTestBridge_TearDownTestUrlLoaderFactoryHelper(
    JNIEnv* env) {
  TestUrlLoaderFactoryHelper::SharedInstance()->TearDown();
}
