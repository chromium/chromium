// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

static const char* kIdTokenAdvancedProtectionEnabled =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSJdIH0="  // payload: { "services": ["tia"] }
    ".dummy-signature";
static const char* kIdTokenAdvancedProtectionDisabled =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";

static const char* kAPTokenFetchStatusMetric =
    "SafeBrowsing.AdvancedProtection.APTokenFetchStatus";
static const char* kTokenFetchStatusMetric =
    "SafeBrowsing.AdvancedProtection.TokenFetchStatus";

// Helper class that ensure RegisterProfilePrefs() is called on the test
// PrefService's registry before the IdentityTestEnvironment constructor
// is invoked.
class TestWithPrefService : public testing::Test {
 public:
  TestWithPrefService() { RegisterProfilePrefs(pref_service_.registry()); }

 protected:
  base::test::TaskEnvironment task_environment;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

class AdvancedProtectionStatusManagerTest : public TestWithPrefService {
 public:
  AdvancedProtectionStatusManagerTest()
      : identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_) {}

  CoreAccountId SignIn(const std::string& email,
                       bool is_under_advanced_protection) {
    AccountInfo account_info = identity_test_env_.MakeAccountAvailable(email);

    account_info.is_under_advanced_protection = is_under_advanced_protection;
    identity_test_env_.SetPrimaryAccount(account_info.email);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    return account_info.account_id;
  }

  void MakeOAuthTokenFetchSucceed(const CoreAccountId& account_id,
                                  bool is_under_advanced_protection) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id, "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1),
        is_under_advanced_protection ? kIdTokenAdvancedProtectionEnabled
                                     : kIdTokenAdvancedProtectionDisabled);
  }

  void MakeOAuthTokenFetchFail(const CoreAccountId& account_id,
                               bool is_transient_error) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        account_id,
        GoogleServiceAuthError(
            is_transient_error
                ? GoogleServiceAuthError::CONNECTION_FAILED
                : GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

 protected:
  signin::IdentityTestEnvironment identity_test_env_;
};

}  // namespace

TEST_F(AdvancedProtectionStatusManagerTest, NotSignedInOnStartUp) {
  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // If user's not signed-in. No refresh is required.
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       SignedInLongTimeAgoRefreshFailTransientError) {
  base::HistogramTester histograms;
  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));

  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id,
                          /* is_transient_error = */ true);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());

  EXPECT_THAT(histograms.GetAllSamples(kTokenFetchStatusMetric),
              testing::ElementsAre(base::Bucket(3 /*CONNECTION_FAILED*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  // A retry should be scheduled.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       SignedInLongTimeAgoRefreshFailNonTransientError) {
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id,
                          /* is_transient_error = */ false);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());

  EXPECT_THAT(
      histograms.GetAllSamples(kTokenFetchStatusMetric),
      testing::ElementsAre(base::Bucket(1 /*INVALID_GAIA_CREDENTIALS*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  // No retry should be scheduled.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, SignedInLongTimeAgoNotUnderAP) {
  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  base::RunLoop().RunUntilIdle();
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));

  EXPECT_THAT(histograms.GetAllSamples(kTokenFetchStatusMetric),
              testing::ElementsAre(base::Bucket(0 /*NONE*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, SignedInLongTimeAgoUnderAP) {
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status yet.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ true);

  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, AlreadySignedInAndUnderAP) {
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // under advanced protection.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());

  // A refresh is scheduled in the future.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, AlreadySignedInAndNotUnderAP) {
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // NOT under advanced protection.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);

  // Incognito profile should share the advanced protection status with the
  // original profile.
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, StayInAdvancedProtection) {
  base::Time last_update = base::Time::Now();
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_update.ToDeltaSinceWindowsEpoch().InMicroseconds());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());

  // Simulate gets refresh token.
  aps_manager.OnGetIDToken(account_id, kIdTokenAdvancedProtectionEnabled);
  EXPECT_GT(
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          pref_service_.GetInt64(prefs::kAdvancedProtectionLastRefreshInUs))),
      last_update);
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

#if !defined(OS_CHROMEOS)
// Not applicable to Chrome OS.
TEST_F(AdvancedProtectionStatusManagerTest, SignInAndSignOutEvent) {
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  SignIn("test@test.com",
         /* is_under_advanced_protection = */ true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}
#endif

TEST_F(AdvancedProtectionStatusManagerTest, AccountRemoval) {
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  // Simulates account update.
  identity_test_env_.identity_manager()
      ->GetAccountsMutator()
      ->UpdateAccountInfo(account_id,
                          /*is_child_account=*/false,
                          /*is_under_advanced_protection=*/true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  identity_test_env_.EnableRemovalOfExtendedAccountInfo();
  identity_test_env_.identity_manager()->GetAccountsMutator()->RemoveAccount(
      account_id,
      signin_metrics::SourceForRefreshTokenOperation::kUserMenu_RemoveAccount);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       AdvancedProtectionDisabledAfterSignin) {
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  // There is no account, so the timer should not run at startup.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);

  // Now that we've signed into Advanced Protection, we should have a scheduled
  // refresh.
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // Skip the 24 hour wait, and try to refresh the token now.
  aps_manager.timer_.FireNow();
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       StartupAfterLongWaitRefreshesImmediately) {
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);
  base::RunLoop().RunUntilIdle();

  base::Time last_refresh_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refresh_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

// On ChromeOS, there is no unconsented primary account. We can only track the
// primary account.
#if !defined(OS_CHROMEOS)
TEST_F(AdvancedProtectionStatusManagerTest, TracksUnconsentedPrimaryAccount) {
  AdvancedProtectionStatusManager aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Sign in, but don't set this as the primary account.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("test@test.com");
  account_info.is_under_advanced_protection = true;
  identity_test_env_.SetCookieAccounts(
      {{account_info.email, account_info.gaia}});
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}
#endif

}  // namespace safe_browsing
