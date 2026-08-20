// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {
namespace {

TEST(ShouldShowAnimatedIdentityOnOpeningWindow, ReturnsFalseForNewWindow) {
  // Setup a testing profile manager with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  std::string name("testing_profile");
  TestingProfile* profile = profile_manager.CreateTestingProfile(
      name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16(name), 0,
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());

  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));

  // Animation is shown once.
  RecordAnimatedIdentityTriggered(profile);

  // Wait a few seconds.
  task_environment.FastForwardBy(base::Seconds(6));

  // Animation is not shown again in a new window.
  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));
}

std::unique_ptr<KeyedService> BuildTestAccountPreviewDataService(
    content::BrowserContext* context) {
  return std::make_unique<signin::TestAccountPreviewDataService>();
}

class SigninUiUtilTest : public testing::Test {
 public:
  SigninUiUtilTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    factories.emplace_back(
        AccountPreviewDataServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestAccountPreviewDataService));
    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile",
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        u"test_profile", 0, std::move(factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  TestingProfile* profile() { return profile_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }
  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }
  signin::TestAccountPreviewDataService* account_preview_data_service() {
    return static_cast<signin::TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(SigninUiUtilTest,
       GetSingleAccountForPromosNoPreferredAccountReturnsDefault) {
  AccountInfo account1 =
      identity_test_env()->MakeAccountAvailable("acc1@gmail.com");
  AccountInfo account2 =
      identity_test_env()->MakeAccountAvailable("acc2@gmail.com");
  signin::SetCookieAccounts(
      identity_manager(), &test_url_loader_factory_,
      {{account1.email, account1.gaia}, {account2.email, account2.gaia}});

  // With no preview data / preference set, returns the default account (first).
  EXPECT_EQ(GetSingleAccountForPromos(identity_manager(),
                                      account_preview_data_service())
                .gaia,
            account1.gaia);
  EXPECT_EQ(GetSingleAccountForPromos(identity_manager(), nullptr).gaia,
            account1.gaia);
}

TEST_F(SigninUiUtilTest,
       GetSingleAccountForPromosPreferredAccountUsedWhenAvailable) {
  AccountInfo account1 =
      identity_test_env()->MakeAccountAvailable("acc1@gmail.com");
  AccountInfo account2 =
      identity_test_env()->MakeAccountAvailable("acc2@gmail.com");
  signin::SetCookieAccounts(
      identity_manager(), &test_url_loader_factory_,
      {{account1.email, account1.gaia}, {account2.email, account2.gaia}});

  // Set preferred account to account2.
  signin::AccountPreviewDataService::AccountPreviewPreference pref;
  pref.gaia_id = account2.gaia;
  account_preview_data_service()->SetPreferredAccountForPromo(pref);

  // When preview service has preferred account, it is returned.
  EXPECT_EQ(GetSingleAccountForPromos(identity_manager(),
                                      account_preview_data_service())
                .gaia,
            account2.gaia);

  // Without preview service, default account (account1) is returned.
  EXPECT_EQ(GetSingleAccountForPromos(identity_manager(), nullptr).gaia,
            account1.gaia);
}

TEST_F(SigninUiUtilTest,
       GetSingleAccountForPromosPreferredAccountInvalidFallsBackToDefault) {
  AccountInfo account1 =
      identity_test_env()->MakeAccountAvailable("acc1@gmail.com");
  signin::SetCookieAccounts(identity_manager(), &test_url_loader_factory_,
                            {{account1.email, account1.gaia}});

  // Set preferred account to an account not in identity manager.
  signin::AccountPreviewDataService::AccountPreviewPreference pref;
  pref.gaia_id = GaiaId("unknown_gaia_id");
  account_preview_data_service()->SetPreferredAccountForPromo(pref);

  // Falls back to default account.
  EXPECT_EQ(GetSingleAccountForPromos(identity_manager(),
                                      account_preview_data_service())
                .gaia,
            account1.gaia);
}

TEST_F(SigninUiUtilTest,
       GetOrderedAccountsForDisplayMovesPreferredAccountToFront) {
  AccountInfo account1 =
      identity_test_env()->MakeAccountAvailable("acc1@gmail.com");
  AccountInfo account2 =
      identity_test_env()->MakeAccountAvailable("acc2@gmail.com");
  AccountInfo account3 =
      identity_test_env()->MakeAccountAvailable("acc3@gmail.com");
  signin::SetCookieAccounts(identity_manager(), &test_url_loader_factory_,
                            {{account1.email, account1.gaia},
                             {account2.email, account2.gaia},
                             {account3.email, account3.gaia}});

  // Set preferred account to account3.
  signin::AccountPreviewDataService::AccountPreviewPreference pref;
  pref.gaia_id = account3.gaia;
  account_preview_data_service()->SetPreferredAccountForPromo(pref);

  std::vector<AccountInfo> ordered = GetOrderedAccountsForDisplay(
      identity_manager(), account_preview_data_service(),
      /*restrict_to_accounts_eligible_for_signin=*/true);
  ASSERT_EQ(ordered.size(), 3u);
  EXPECT_EQ(ordered[0].gaia, account3.gaia);
  EXPECT_EQ(ordered[1].gaia, account1.gaia);
  EXPECT_EQ(ordered[2].gaia, account2.gaia);
}

}  // namespace
}  // namespace signin_ui_util
