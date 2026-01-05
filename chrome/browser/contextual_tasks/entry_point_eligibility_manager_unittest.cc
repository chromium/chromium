// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace {

class MockContextualTasksUiService : public ContextualTasksUiService {
 public:
  MockContextualTasksUiService(Profile* profile,
                               ContextualTasksService* contextual_tasks_service,
                               signin::IdentityManager* identity_manager)
      : ContextualTasksUiService(profile,
                                 contextual_tasks_service,
                                 identity_manager) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(bool, IsSignedInToBrowserWithValidCredentials, (), (override));
};

std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TestSigninClient>(profile->GetPrefs());
}

}  // namespace

class EntryPointEligibilityManagerTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    builder.AddTestingFactory(ChromeSigninClientFactory::GetInstance(),
                              base::BindRepeating(&BuildTestSigninClient));
    builder.AddTestingFactory(
        ContextualTasksServiceFactory::GetInstance(),
        base::BindRepeating(
            &EntryPointEligibilityManagerTest::CreateMockContextualTasksService,
            base::Unretained(this)));
    builder.AddTestingFactory(
        ContextualTasksUiServiceFactory::GetInstance(),
        base::BindRepeating(
            &EntryPointEligibilityManagerTest::CreateMockUiService,
            base::Unretained(this)));
    builder.AddTestingFactory(
        AimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(
            &EntryPointEligibilityManagerTest::CreateMockAimEligibilityService,
            base::Unretained(this)));

    profile_ = builder.Build();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    auto* signin_client = static_cast<TestSigninClient*>(
        identity_test_env_adaptor_->identity_test_env()->signin_client());
    identity_test_env_adaptor_->identity_test_env()->SetTestURLLoaderFactory(
        signin_client->GetTestURLLoaderFactory());

    // Ensure mocked services are created so that pointers are populated.
    ContextualTasksServiceFactory::GetForProfile(profile_.get());
    ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get());
    AimEligibilityServiceFactory::GetForProfile(profile_.get());

    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));
  }

  std::unique_ptr<KeyedService> CreateMockContextualTasksService(
      content::BrowserContext* context) {
    auto mock = std::make_unique<NiceMock<MockContextualTasksService>>();
    mock_contextual_tasks_service_ = mock.get();
    return mock;
  }

  std::unique_ptr<KeyedService> CreateMockUiService(
      content::BrowserContext* context) {
    auto mock = std::make_unique<NiceMock<MockContextualTasksUiService>>(
        Profile::FromBrowserContext(context), mock_contextual_tasks_service_,
        IdentityManagerFactory::GetForProfile(
            Profile::FromBrowserContext(context)));
    mock_ui_service_ = mock.get();
    return mock;
  }

  std::unique_ptr<KeyedService> CreateMockAimEligibilityService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto mock = std::make_unique<NiceMock<MockAimEligibilityService>>(
        *profile->GetPrefs(), nullptr, nullptr,
        IdentityManagerFactory::GetForProfile(profile));
    mock_aim_service_ = mock.get();
    return mock;
  }

  void InitializeManager() {
    manager_ = std::make_unique<EntryPointEligibilityManager>(
        &mock_browser_window_interface_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  raw_ptr<MockContextualTasksService> mock_contextual_tasks_service_ = nullptr;
  raw_ptr<MockContextualTasksUiService> mock_ui_service_ = nullptr;
  raw_ptr<MockAimEligibilityService> mock_aim_service_ = nullptr;

  std::unique_ptr<EntryPointEligibilityManager> manager_;
};

// TODO(crbug.com/472938809): Consistently failing on Linux UBSan builder.
#if BUILDFLAG(IS_LINUX) && defined(UNDEFINED_SANITIZER)
#define MAYBE_AreEntryPointsEligible_True DISABLED_AreEntryPointsEligible_True
#else
#define MAYBE_AreEntryPointsEligible_True AreEntryPointsEligible_True
#endif
TEST_F(EntryPointEligibilityManagerTest, MAYBE_AreEntryPointsEligible_True) {
  // Setup all conditions to true.
  auto account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));

  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = true;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));

  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 0);  // Allowed

  InitializeManager();

  EXPECT_TRUE(manager_->AreEntryPointsEligible());
}

TEST_F(EntryPointEligibilityManagerTest, AreEntryPointsEligible_NotSignedIn) {
  // Signed out.
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(false));

  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = true;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));

  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 0);

  InitializeManager();

  EXPECT_FALSE(manager_->AreEntryPointsEligible());
}

TEST_F(EntryPointEligibilityManagerTest, AreEntryPointsEligible_CookieMissing) {
  // Signed in but cookie missing.
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  // Cookie jar empty.

  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));

  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = true;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));

  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 0);

  InitializeManager();

  EXPECT_FALSE(manager_->AreEntryPointsEligible());
}
TEST_F(EntryPointEligibilityManagerTest,
       AreEntryPointsEligible_FeatureDisabled) {
  auto account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));

  // Feature disabled.
  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = false;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));

  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 0);

  InitializeManager();

  EXPECT_FALSE(manager_->AreEntryPointsEligible());
}

TEST_F(EntryPointEligibilityManagerTest, AreEntryPointsEligible_AimNotAllowed) {
  auto account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));

  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = true;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));

  // Policy disabled.
  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 1);  // Disabled

  InitializeManager();

  EXPECT_FALSE(manager_->AreEntryPointsEligible());
}

// Disable test on ChromeOS since ChromeOS does not support switching the
// primary account without a restart.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NotifyEntryPointEligibilityChanged \
  DISABLED_NotifyEntryPointEligibilityChanged
#else
#define MAYBE_NotifyEntryPointEligibilityChanged \
  NotifyEntryPointEligibilityChanged
#endif
TEST_F(EntryPointEligibilityManagerTest,
       MAYBE_NotifyEntryPointEligibilityChanged) {
  // Start with eligible state.
  auto account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("test@example.com",
                                        signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));
  FeatureEligibility eligibility;
  eligibility.contextual_tasks_enabled = true;
  eligibility.aim_eligible = true;
  eligibility.context_sharing_enabled = true;
  EXPECT_CALL(*mock_contextual_tasks_service_, GetFeatureEligibility())
      .WillRepeatedly(Return(eligibility));
  profile_->GetPrefs()->SetInteger(omnibox::kAIModeSettings, 0);

  InitializeManager();
  ASSERT_TRUE(manager_->AreEntryPointsEligible());

  // Register observer.
  std::optional<bool> notified_eligibility;
  auto subscription = manager_->RegisterOnEntryPointEligibilityChanged(
      base::BindLambdaForTesting(
          [&](bool eligible) { notified_eligibility = eligible; }));

  // 1. Test OnAccountsInCookieUpdated: Set cookies to empty.
  notified_eligibility.reset();
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts({});
  EXPECT_EQ(notified_eligibility, false);

  // Restore eligible state.
  notified_eligibility.reset();
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});
  EXPECT_EQ(notified_eligibility, true);

  // 2. Test OnPrimaryAccountChanged: Clear primary account.
  notified_eligibility.reset();
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  EXPECT_EQ(notified_eligibility, false);

  // Restore eligible state.
  notified_eligibility.reset();
  account_info = identity_test_env_adaptor_->identity_test_env()
                     ->MakePrimaryAccountAvailable(
                         "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});
  EXPECT_EQ(notified_eligibility, true);

  // 3. Test OnRefreshTokenUpdatedForAccount:
  // Change IsSignedIn expectation to false to simulate invalid credentials.
  notified_eligibility.reset();
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(false));
  identity_test_env_adaptor_->identity_test_env()
      ->SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_EQ(notified_eligibility, false);

  // Restore.
  notified_eligibility.reset();
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));
  identity_test_env_adaptor_->identity_test_env()
      ->SetRefreshTokenForPrimaryAccount();
  EXPECT_EQ(notified_eligibility, true);

  // 4. Test OnRefreshTokenRemovedForAccount:
  notified_eligibility.reset();
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(false));
  identity_test_env_adaptor_->identity_test_env()
      ->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_EQ(notified_eligibility, false);

  // Restore.
  notified_eligibility.reset();
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(true));
  identity_test_env_adaptor_->identity_test_env()
      ->SetRefreshTokenForPrimaryAccount();
  EXPECT_EQ(notified_eligibility, true);

  // 5. Test OnErrorStateOfRefreshTokenUpdatedForAccount:
  notified_eligibility.reset();
  EXPECT_CALL(*mock_ui_service_, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(Return(false));
  identity_test_env_adaptor_->identity_test_env()
      ->UpdatePersistentErrorOfRefreshTokenForAccount(
          account_info.account_id,
          GoogleServiceAuthError(
              GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(notified_eligibility, false);
}

}  // namespace contextual_tasks
