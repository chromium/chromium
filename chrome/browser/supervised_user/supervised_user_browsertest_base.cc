// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace supervised_user {

namespace {
std::unique_ptr<KeyedService> BuildSupervisedUserService(
    MockUrlCheckerClient& mock_url_checker_client,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FamilyLinkSettingsService& settings_service =
      CHECK_DEREF(FamilyLinkSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()));

  return std::make_unique<SupervisedUserService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *profile->GetPrefs(), settings_service,
      SyncServiceFactory::GetForProfile(profile),
      std::make_unique<FamilyLinkUrlFilter>(
          settings_service, *profile->GetPrefs(),
          std::make_unique<FakeURLFilterDelegate>(),
          std::make_unique<UrlCheckerClientWrapper>(mock_url_checker_client)),
      std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
      g_browser_process->device_parental_controls());
}

std::unique_ptr<KeyedService> BuildSupervisedUserUrlFilteringService(
    MockUrlCheckerClient& mock_url_checker_client,
    content::BrowserContext* context) {
  return std::make_unique<SupervisedUserUrlFilteringService>(
      CHECK_DEREF(SupervisedUserServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          g_browser_process->device_parental_controls(),
          std::make_unique<UrlCheckerClientWrapper>(mock_url_checker_client)));
}
}  // namespace

SupervisedUserBrowserTestBase::SupervisedUserBrowserTestBase() = default;
SupervisedUserBrowserTestBase::~SupervisedUserBrowserTestBase() = default;

void SupervisedUserBrowserTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_ANDROID)
  AndroidBrowserTest::SetUpBrowserContextKeyedServices(context);
#else
  MixinBasedInProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
#endif  // BUILDFLAG(IS_ANDROID)

  // Preset user-prefs before any service is initialized.
  Profile* profile = Profile::FromBrowserContext(context);
  if (initial_state_.family_link_parental_controls) {
    EnableParentalControls(*profile->GetPrefs());
  } else {
    DisableParentalControls(*profile->GetPrefs());
  }

#if BUILDFLAG(IS_ANDROID)
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(
      initial_state_.android_parental_controls.browser_filter);
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(
      initial_state_.android_parental_controls.search_filter);
#endif  // BUILDFLAG(IS_ANDROID)

  SupervisedUserServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildSupervisedUserService,
                                   std::ref(mock_url_checker_client_)));

  SupervisedUserUrlFilteringServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildSupervisedUserUrlFilteringService,
                                   std::ref(mock_url_checker_client_)));

  browser_context_keyed_services_set_up_ = true;
}

void SupervisedUserBrowserTestBase::SetInitialSupervisedUserState(
    InitialSupervisedUserState initial_state) {
  CHECK(!browser_context_keyed_services_set_up_)
      << "SetInitialSupervisedUserState must be called before setting up the "
         "mocks.";

  initial_state_ = initial_state;
}

SupervisedUserService* SupervisedUserBrowserTestBase::GetSupervisedUserService()
    const {
  return SupervisedUserServiceFactory::GetForProfile(GetProfile());
}
SupervisedUserUrlFilteringService*
SupervisedUserBrowserTestBase::GetSupervisedUserUrlFilteringService() const {
  return SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile());
}

MockUrlCheckerClient& SupervisedUserBrowserTestBase::GetMockUrlCheckerClient() {
  return mock_url_checker_client_;
}

#if BUILDFLAG(IS_ANDROID)
AndroidParentalControls&
SupervisedUserBrowserTestBase::GetDeviceParentalControls() {
  return static_cast<AndroidParentalControls&>(
      g_browser_process->device_parental_controls());
}
#else
DeviceParentalControls&
SupervisedUserBrowserTestBase::GetDeviceParentalControls() {
  return g_browser_process->device_parental_controls();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace supervised_user
