// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/turn_sync_on_helper.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

using signin::ConsentLevel;
using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace {

const char kSyncFirstRunCompleted[] = "sync.first_run_completed";

class TestDelegate : public TurnSyncOnHelper::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  void ShowSyncConfirmation(Profile* profile, Browser* browser) override {
    show_sync_confirmation_count_++;
  }

  void ShowSyncSettings(Profile* profile, Browser* browser) override {
    show_sync_settings_count_++;
  }

  int show_sync_confirmation_count_ = 0;
  int show_sync_settings_count_ = 0;
};

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class TurnSyncOnHelperTest : public BrowserWithTestWindowTest {
 public:
  TurnSyncOnHelperTest() {
    feature_list_.InitWithFeatures({chromeos::features::kSplitSettingsSync,
                                    chromeos::features::kUseBrowserSyncConsent},
                                   {});
  }
  ~TurnSyncOnHelperTest() override = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    sync_service_ = static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));
    // Reset the sync service to the pre-setup state.
    sync_service_->SetFirstSetupComplete(false);
    sync_service_->GetUserSettings()->SetSyncRequested(false);
  }

  void TearDown() override {
    // Must be destroyed before profile().
    identity_test_env_profile_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  syncer::TestSyncService* sync_service_ = nullptr;
};

TEST_F(TurnSyncOnHelperTest, UserAcceptsDefaults) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Sync confirmation dialog is shown
  EXPECT_TRUE(sync_service_->GetUserSettings()->IsSyncRequested());
  EXPECT_EQ(1, delegate->show_sync_confirmation_count_);

  // Simulate the user clicking "Yes, I'm in".
  helper.OnSyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // Setup is complete and we didn't show settings.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(sync_service_->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_EQ(0, delegate->show_sync_settings_count_);
}

TEST_F(TurnSyncOnHelperTest, UserClicksSettings) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Simulate the user clicking "Settings".
  helper.OnSyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);

  // Setup is not complete and we opened settings.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_FALSE(sync_service_->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_EQ(1, delegate->show_sync_settings_count_);
}

TEST_F(TurnSyncOnHelperTest, UserClicksCancel) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Simulate the user clicking "Cancel".
  helper.OnSyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  // Setup is not complete and we didn't show settings.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_FALSE(sync_service_->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_EQ(0, delegate->show_sync_settings_count_);
}

TEST_F(TurnSyncOnHelperTest, UserClosesUI) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Simulate the user closing the consent UI.
  helper.OnSyncConfirmationUIClosed(LoginUIService::UI_CLOSED);

  // Setup is not complete and we didn't show settings.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_FALSE(sync_service_->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_EQ(0, delegate->show_sync_settings_count_);
}

TEST_F(TurnSyncOnHelperTest, UserPreviouslyDeclinedSync) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  // Simulate a user who previously completed the first-run flow.
  profile()->GetPrefs()->SetBoolean(kSyncFirstRunCompleted, true);

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Sync confirmation dialog isn't shown.
  EXPECT_EQ(0, delegate->show_sync_confirmation_count_);
}

TEST_F(TurnSyncOnHelperTest, UserPreviouslyAcceptedSync) {
  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com");
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));

  auto test_delegate = std::make_unique<TestDelegate>();
  TestDelegate* delegate = test_delegate.get();
  TurnSyncOnHelper helper(profile(), std::move(test_delegate));

  // Simulate the first browser window becoming active.
  BrowserList::SetLastActive(browser());

  // Sync confirmation dialog isn't shown.
  EXPECT_EQ(0, delegate->show_sync_confirmation_count_);
}

TEST_F(TurnSyncOnHelperTest, UrlKeyedMetricsConsent) {
  identity_test_env()->MakeUnconsentedPrimaryAccountAvailable("user@gmail.com");

  // User is not consented by default.
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile()->GetPrefs());
  ASSERT_FALSE(consent_helper->IsEnabled());

  // Simulate user consenting to sync.
  TurnSyncOnHelper helper(profile(), std::make_unique<TestDelegate>());
  BrowserList::SetLastActive(browser());
  helper.OnSyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // URL keyed metrics are enabled.
  EXPECT_TRUE(consent_helper->IsEnabled());
}

}  // namespace
