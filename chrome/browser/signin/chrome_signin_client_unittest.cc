// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/browser_with_test_window_test.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

class MockChromeSigninClient : public ChromeSigninClient {
 public:
  explicit MockChromeSigninClient(Profile* profile)
      : ChromeSigninClient(profile) {}

  MOCK_METHOD1(ShowUserManager, void(const base::FilePath&));
  MOCK_METHOD1(LockForceSigninProfile, void(const base::FilePath&));

  MOCK_METHOD2(SignOutCallback,
               void(signin_metrics::ProfileSignout,
                    SigninClient::SignoutDecision signout_decision));

  MOCK_METHOD0(GetAllBookmarksCount, std::optional<size_t>());
  MOCK_METHOD0(GetBookmarkBarBookmarksCount, std::optional<size_t>());
  MOCK_METHOD0(GetExtensionsCount, std::optional<size_t>());
};

class ChromeSigninClientSignoutTest : public BrowserWithTestWindowTest {
 public:
  ChromeSigninClientSignoutTest() : forced_signin_setter_(true) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    CreateClient(browser()->profile());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
  }

  void CreateClient(Profile* profile) {
    client_ = std::make_unique<MockChromeSigninClient>(profile);
  }

  void PreSignOut(signin_metrics::ProfileSignout source_metric) {
    client_->PreSignOut(
        base::BindOnce(&MockChromeSigninClient::SignOutCallback,
                       base::Unretained(client_.get()), source_metric),
        source_metric);
  }

  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter_;
  std::unique_ptr<MockChromeSigninClient> client_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, SignOutCallback(source_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(source_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(false);
  CreateClient(browser()->profile());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, SignOutCallback(source_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);
  PreSignOut(source_metric);
}

TEST_F(ChromeSigninClientSignoutTest, AllAllowed) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  EXPECT_FALSE(profile->IsChild());

  CreateClient(profile.get());

  EXPECT_TRUE(client_->IsClearPrimaryAccountAllowed());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
#endif
}

TEST_F(ChromeSigninClientSignoutTest, ChildProfile) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(profile->IsChild());

  CreateClient(profile.get());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(client_->IsClearPrimaryAccountAllowed());
#else
  EXPECT_TRUE(client_->IsClearPrimaryAccountAllowed());
#endif
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
}

class ChromeSigninClientSignoutSourceTest
    : public ::testing::WithParamInterface<signin_metrics::ProfileSignout>,
      public ChromeSigninClientSignoutTest {
 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  signin::IdentityTestEnvironment identity_test_env_;
};

// Returns true if signout can be disallowed by policy for the given source.
bool IsAlwaysAllowedSignoutSources(
    signin_metrics::ProfileSignout signout_source) {
  switch (signout_source) {
    // NOTE: SIGNOUT_TEST == SIGNOUT_PREF_CHANGED.
    case signin_metrics::ProfileSignout::kPrefChanged:
    case signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged:
    case signin_metrics::ProfileSignout::kUserClickedSignoutSettings:
    case signin_metrics::ProfileSignout::kServerForcedDisable:
    case signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin:
    case signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit:
    case signin_metrics::ProfileSignout::kSigninRetriggered:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage:
    case signin_metrics::ProfileSignout::
        kIosAccountRemovedFromDeviceAfterRestore:
    case signin_metrics::ProfileSignout::kUserDeletedAccountCookies:
    case signin_metrics::ProfileSignout::kGaiaCookieUpdated:
    case signin_metrics::ProfileSignout::kAccountReconcilorReconcile:
    case signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu:
    case signin_metrics::ProfileSignout::kAccountEmailUpdated:
    case signin_metrics::ProfileSignout::kSigninManagerUpdateUPA:
    case signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn:
    case signin_metrics::ProfileSignout::
        kUserDeclinedHistorySyncAfterDedicatedSignIn:
    case signin_metrics::ProfileSignout::kDeviceLockRemovedOnAutomotive:
    case signin_metrics::ProfileSignout::kRevokeSyncFromSettings:
    case signin_metrics::ProfileSignout::kIdleTimeoutPolicyTriggeredSignOut:
    case signin_metrics::ProfileSignout::kSignoutForAccountSwitching:
    case signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu:
    case signin_metrics::ProfileSignout::kUserDisabledAllowChromeSignIn:
    case signin_metrics::ProfileSignout::kSignoutBeforeSupervisedSignin:
    case signin_metrics::ProfileSignout::kSignoutFromWidgets:
    case signin_metrics::ProfileSignout::kForcedDiceMigration:
      return false;

    case signin_metrics::ProfileSignout::kAccountRemovedFromDevice:
    // Allow signout because data has not been synced yet.
    case signin_metrics::ProfileSignout::kAbortSignin:
    case signin_metrics::ProfileSignout::
        kCancelSyncConfirmationOnWebOnlySignedIn:
    case signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount:
    case signin_metrics::ProfileSignout::kMovePrimaryAccount:
    // Allow signout for tests that want to force it.
    case signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest:
    case signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog:
    case signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion:
    case signin_metrics::ProfileSignout::
        kUserDeclinedEnterpriseManagementDisclaimer:
      return true;
  }
}

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutAllowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_TRUE(client_->IsClearPrimaryAccountAllowed());
  ASSERT_TRUE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is always allowed.
  EXPECT_CALL(*client_, SignOutCallback(signout_source,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(signout_source);
}

// TODO(crbug.com/40240718): Enable |ChromeSigninClientSignoutSourceTest| test
// suite on Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  ASSERT_FALSE(client_->IsClearPrimaryAccountAllowed());

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, signout_decision))
      .Times(1);

  PreSignOut(signout_source);
}

TEST_P(ChromeSigninClientSignoutSourceTest, RevokeSyncDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);
  ASSERT_FALSE(client_->IsClearPrimaryAccountAllowed());
  ASSERT_FALSE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, signout_decision))
      .Times(1);

  PreSignOut(signout_source);
}
#endif

const signin_metrics::ProfileSignout kSignoutSources[] = {
    signin_metrics::ProfileSignout::kPrefChanged,
    signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged,
    signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
    signin_metrics::ProfileSignout::kAbortSignin,
    signin_metrics::ProfileSignout::kServerForcedDisable,
    signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin,
    signin_metrics::ProfileSignout::kAccountRemovedFromDevice,
    signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit,
    signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest,
    signin_metrics::ProfileSignout::kUserDeletedAccountCookies,
    signin_metrics::ProfileSignout::kIosAccountRemovedFromDeviceAfterRestore,
    signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings,
    signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
    signin_metrics::ProfileSignout::kSigninRetriggered,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog,
    signin_metrics::ProfileSignout::kAccountEmailUpdated,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage,
    signin_metrics::ProfileSignout::kGaiaCookieUpdated,
    signin_metrics::ProfileSignout::kAccountReconcilorReconcile,
    signin_metrics::ProfileSignout::kSigninManagerUpdateUPA,
    signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn,
    signin_metrics::ProfileSignout::
        kUserDeclinedHistorySyncAfterDedicatedSignIn,
    signin_metrics::ProfileSignout::kDeviceLockRemovedOnAutomotive,
    signin_metrics::ProfileSignout::kRevokeSyncFromSettings,
    signin_metrics::ProfileSignout::kCancelSyncConfirmationOnWebOnlySignedIn,
    signin_metrics::ProfileSignout::kIdleTimeoutPolicyTriggeredSignOut,
    signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount,
    signin_metrics::ProfileSignout::kMovePrimaryAccount,
    signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion,
    signin_metrics::ProfileSignout::kSignoutForAccountSwitching,
    signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu,
    signin_metrics::ProfileSignout::kUserDisabledAllowChromeSignIn,
    signin_metrics::ProfileSignout::kSignoutBeforeSupervisedSignin,
    signin_metrics::ProfileSignout::kSignoutFromWidgets,
    signin_metrics::ProfileSignout::kUserDeclinedEnterpriseManagementDisclaimer,
    signin_metrics::ProfileSignout::kForcedDiceMigration,
};

// kNumberOfObsoleteSignoutSources should be updated when a ProfileSignout
// value is deprecated.
const int kNumberOfObsoleteSignoutSources = 6;
static_assert(std::size(kSignoutSources) + kNumberOfObsoleteSignoutSources ==
                  static_cast<int>(signin_metrics::ProfileSignout::kMaxValue) +
                      1,
              "kSignoutSources should enumerate all ProfileSignout values that "
              "are not obsolete");

INSTANTIATE_TEST_SUITE_P(AllSignoutSources,
                         ChromeSigninClientSignoutSourceTest,
                         testing::ValuesIn(kSignoutSources));

#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
