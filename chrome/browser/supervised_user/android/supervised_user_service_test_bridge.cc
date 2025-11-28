// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "content/public/browser/storage_partition.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserServiceTestBridge_jni.h"

namespace supervised_user {

namespace {
std::unique_ptr<KeyedService> BuildSupervisedUserService(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Test Supervised User Service also substitutes the content filters with
  // fakes.
  return std::make_unique<TestSupervisedUserService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *profile->GetPrefs(),
      *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()),
      SupervisedUserContentFiltersServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()),
      SyncServiceFactory::GetInstance()->GetForProfile(profile),
      std::make_unique<SupervisedUserURLFilter>(
          *profile->GetPrefs(), std::make_unique<FakeURLFilterDelegate>(),
          std::make_unique<safe_search_api::FakeURLCheckerClient>()),
      std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
      InitialSupervisionState::kUnsupervised);
}

TestSupervisedUserService* GetTestSupervisedUserService(Profile* profile) {
  return static_cast<TestSupervisedUserService*>(
      SupervisedUserServiceFactory::GetInstance()->GetForBrowserContext(
          profile));
}

}  // namespace

static void JNI_SupervisedUserServiceTestBridge_Init(JNIEnv* env,
                                                     Profile* profile) {
  SupervisedUserServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile, base::BindRepeating(&BuildSupervisedUserService));
}

static void JNI_SupervisedUserServiceTestBridge_EnableBrowserContentFilters(
    JNIEnv* env,
    Profile* profile) {
  GetTestSupervisedUserService(profile)
      ->browser_content_filters_observer_weak_ptr()
      ->SetEnabled(true);
}

static void JNI_SupervisedUserServiceTestBridge_EnableSearchContentFilters(
    JNIEnv* env,
    Profile* profile) {
  GetTestSupervisedUserService(profile)
      ->search_content_filters_observer_weak_ptr()
      ->SetEnabled(true);
}
}  // namespace supervised_user

DEFINE_JNI(SupervisedUserServiceTestBridge)
