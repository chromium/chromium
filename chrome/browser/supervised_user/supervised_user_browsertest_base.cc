// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/child_accounts/list_family_members_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace supervised_user {

namespace {
// A URLCheckerClient that wraps a MockUrlCheckerClient. Effectively, this
// allows the URLChecker to get a wrapper of the mock (as a WeakPtr) without
// giving up ownership of the mock.
class WrappedUrlCheckerClient : public safe_search_api::URLCheckerClient {
 public:
  explicit WrappedUrlCheckerClient(base::WeakPtr<MockUrlCheckerClient> client)
      : client_(std::move(client)) {}
  ~WrappedUrlCheckerClient() override = default;

  void CheckURL(const GURL& url, ClientCheckCallback callback) override {
    client_->CheckURL(url, std::move(callback));
  }

 private:
  base::WeakPtr<MockUrlCheckerClient> client_;
};

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<ContentFiltersObserverBridge>
CreateFakeContentFiltersObserverBridge(const std::string& setting_name,
                                       const PrefService* pref_service,
                                       bool enabled) {
  // Create the bridge and configure its initial value before passing
  // ownership to the SupervisedUserService.
  std::unique_ptr<ContentFiltersObserverBridge> bridge =
      std::make_unique<FakeContentFiltersObserverBridge>(setting_name,
                                                         pref_service);
  bridge->SetEnabledForTesting(enabled);
  return bridge;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<KeyedService> BuildSupervisedUserService(
    base::WeakPtr<MockUrlCheckerClient> mock_url_checker_client,
    SupervisedUserBrowserTestBase::InitialSupervisedUserState initial_state,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  ProfileKey* profile_key = profile->GetProfileKey();

  return std::make_unique<SupervisedUserService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      *profile->GetPrefs(),
      *SupervisedUserSettingsServiceFactory::GetForKey(profile_key),
      SupervisedUserContentFiltersServiceFactory::GetForKey(profile_key),
      SyncServiceFactory::GetForProfile(profile),
      std::make_unique<SupervisedUserURLFilter>(
          *profile->GetPrefs(), std::make_unique<FakeURLFilterDelegate>(),
          std::make_unique<WrappedUrlCheckerClient>(mock_url_checker_client)),
      std::make_unique<SupervisedUserServicePlatformDelegate>(*profile)
#if BUILDFLAG(IS_ANDROID)
          ,
      CreateFakeContentFiltersObserverBridge(
          kBrowserContentFiltersSettingName, profile->GetPrefs(),
          initial_state.android_parental_controls.browser_filter),
      CreateFakeContentFiltersObserverBridge(
          kSearchContentFiltersSettingName, profile->GetPrefs(),
          initial_state.android_parental_controls.search_filter)
#endif  // BUILDFLAG(IS_ANDROID)
  );
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

  SupervisedUserServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildSupervisedUserService,
                                   mock_url_checker_client_.GetWeakPtr(),
                                   initial_state_));

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

MockUrlCheckerClient& SupervisedUserBrowserTestBase::GetMockUrlCheckerClient() {
  return mock_url_checker_client_;
}

MockUrlCheckerClient::MockUrlCheckerClient() = default;
MockUrlCheckerClient::~MockUrlCheckerClient() = default;

base::WeakPtr<MockUrlCheckerClient> MockUrlCheckerClient::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_ANDROID)
base::WeakPtr<ContentFiltersObserverBridge>
SupervisedUserBrowserTestBase::GetBrowserContentFiltersObserverWeakPtr() const {
  return GetSupervisedUserService()
      ->GetBrowserContentFiltersObserverWeakPtrForTesting();
}

base::WeakPtr<ContentFiltersObserverBridge>
SupervisedUserBrowserTestBase::GetSearchContentFiltersObserverWeakPtr() const {
  return GetSupervisedUserService()
      ->GetSearchContentFiltersObserverWeakPtrForTesting();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace supervised_user
