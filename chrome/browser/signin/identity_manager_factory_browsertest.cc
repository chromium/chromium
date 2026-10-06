// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Property;

constexpr char kTestEmail[] = "test@example.com";

class IdentityManagerFactoryIsolatedModeBrowserTest
    : public InProcessBrowserTest {
 public:
  IdentityManagerFactoryIsolatedModeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  }

 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    SetupIsolatedMode(Profile::FromBrowserContext(context));
  }

  // Returns the IdentityManager for the regular profile.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  }

  CoreAccountId GetMainAccountID() {
    return GetIdentityManager()->PickAccountIdForAccount(
        signin::GetTestGaiaIdForEmail(kTestEmail), kTestEmail);
  }

  CoreAccountInfo GetMainAccountInfo() {
    CoreAccountInfo account_info;
    account_info.email = kTestEmail;
    account_info.gaia = signin::GetTestGaiaIdForEmail(kTestEmail);
    account_info.account_id = GetMainAccountID();
    return account_info;
  }

 private:
  void SetupIsolatedMode(Profile* profile) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetInteger(
        enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
        static_cast<int>(
            enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IdentityManagerFactoryIsolatedModeBrowserTest,
                       PRE_IsolatedModeIdentityManagerInitialization) {
  Profile* parent_profile = browser()->GetProfile();
  ASSERT_TRUE(parent_profile);
  ASSERT_FALSE(parent_profile->IsOffTheRecord());

  signin::IdentityManager* parent_identity_manager = GetIdentityManager();
  ASSERT_TRUE(parent_identity_manager);

  // Sign in the parent profile with a primary account and refresh token prior
  // to creating the Isolated Mode profile.
  signin::MakePrimaryAccountAvailable(
      parent_identity_manager, kTestEmail, signin::ConsentLevel::kSignin);
  ASSERT_TRUE(parent_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_EQ(parent_identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            GetMainAccountID());
  ASSERT_TRUE(parent_identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      parent_identity_manager->HasAccountWithRefreshToken(GetMainAccountID()));
  ASSERT_FALSE(
      parent_identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          GetMainAccountID()));
  ASSERT_THAT(parent_identity_manager->GetAccountsWithRefreshTokens(),
              Contains(GetMainAccountInfo()));
  ASSERT_THAT(parent_identity_manager
                  ->GetExtendedAccountInfoForAccountsWithRefreshToken(),
              Contains(Property(&AccountInfo::GetCoreAccountInfo,
                                GetMainAccountInfo())));

  // Create the Isolated Mode profile and initialize its IdentityManager.
  Profile* isolated_profile =
      parent_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(isolated_profile);
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());

  signin::IdentityManager* isolated_identity_manager =
      IdentityManagerFactory::GetForProfile(isolated_profile);
  ASSERT_TRUE(isolated_identity_manager);
  ASSERT_NE(isolated_identity_manager, parent_identity_manager);
  signin::WaitForRefreshTokensLoaded(isolated_identity_manager);

  // Verify that the IdentityManager in Isolated Mode has no primary account and
  // no accounts with refresh tokens.
  EXPECT_FALSE(isolated_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(isolated_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  EXPECT_THAT(isolated_identity_manager->GetAccountsWithRefreshTokens(),
              IsEmpty());
  EXPECT_THAT(isolated_identity_manager
                  ->GetExtendedAccountInfoForAccountsWithRefreshToken(),
              IsEmpty());

  // Verify that initializing IdentityManager in the Isolated Mode profile did
  // not remove or invalidate the parent profile's primary account or refresh
  // tokens.
  EXPECT_TRUE(parent_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_EQ(parent_identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            GetMainAccountID());
  EXPECT_TRUE(parent_identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      parent_identity_manager->HasAccountWithRefreshToken(GetMainAccountID()));
  EXPECT_FALSE(
      parent_identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          GetMainAccountID()));
  EXPECT_THAT(parent_identity_manager->GetAccountsWithRefreshTokens(),
              Contains(GetMainAccountInfo()));
  EXPECT_THAT(parent_identity_manager
                  ->GetExtendedAccountInfoForAccountsWithRefreshToken(),
              Contains(Property(&AccountInfo::GetCoreAccountInfo,
                                GetMainAccountInfo())));
}

IN_PROC_BROWSER_TEST_F(IdentityManagerFactoryIsolatedModeBrowserTest,
                       IsolatedModeIdentityManagerInitialization) {
  Profile* parent_profile = browser()->GetProfile();
  ASSERT_TRUE(parent_profile);
  ASSERT_FALSE(parent_profile->IsOffTheRecord());

  signin::IdentityManager* parent_identity_manager = GetIdentityManager();
  ASSERT_TRUE(parent_identity_manager);
  signin::WaitForRefreshTokensLoaded(parent_identity_manager);

  // Verify that initializing IdentityManager in the Isolated Mode profile in
  // the PRE_ test did not remove or invalidate the parent profile's persisted
  // primary account or refresh tokens across browser restarts.
  EXPECT_TRUE(parent_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_EQ(parent_identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            GetMainAccountID());
  EXPECT_TRUE(parent_identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      parent_identity_manager->HasAccountWithRefreshToken(GetMainAccountID()));
  EXPECT_FALSE(
      parent_identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          GetMainAccountID()));
  EXPECT_THAT(parent_identity_manager->GetAccountsWithRefreshTokens(),
              Contains(GetMainAccountInfo()));
  EXPECT_THAT(parent_identity_manager
                  ->GetExtendedAccountInfoForAccountsWithRefreshToken(),
              Contains(Property(&AccountInfo::GetCoreAccountInfo,
                                GetMainAccountInfo())));
}

}  // namespace
