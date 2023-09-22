// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/android/jni_string.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserSettingsBridge_jni.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

using base::android::JavaParamRef;

void JNI_SupervisedUserSettingsBridge_SetFilteringBehavior(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint setting) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(setting));
}

void JNI_SupervisedUserSettingsBridge_SetManualFilterForHost(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& host,
    jboolean allowlist) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
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

void JNI_SupervisedUserSettingsBridge_SetSafeSearchResponseForTesting(  // IN-TEST
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean is_allowed) {
  kids_chrome_management::ClassifyUrlResponse response;
  auto url_classification =
      is_allowed ? kids_chrome_management::ClassifyUrlResponse::ALLOWED
                 : kids_chrome_management::ClassifyUrlResponse::RESTRICTED;

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

  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  std::unique_ptr<KidsChromeManagementClient>
      test_kids_chrome_management_client_ =
          std::make_unique<KidsChromeManagementClient>(
              shared_url_loader_factory, identity_manager);

  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  supervised_user_service->GetURLFilter()->InitAsyncURLChecker(
      test_kids_chrome_management_client_.get());
}

void JNI_SupervisedUserSettingsBridge_SetUpTestUrlLoaderFactoryHelper(
    JNIEnv* env) {
  TestUrlLoaderFactoryHelper::SharedInstance()->SetUp();
}

void JNI_SupervisedUserSettingsBridge_TearDownTestUrlLoaderFactoryHelper(
    JNIEnv* env) {
  TestUrlLoaderFactoryHelper::SharedInstance()->TearDown();
}
