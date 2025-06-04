// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_desktop.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
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

static const char* kAPEnabledMetric =
    "SafeBrowsing.Desktop.AdvancedProtection.Enabled";

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

class AdvancedProtectionStatusManagerDesktopTest : public TestWithPrefService {
 public:
  AdvancedProtectionStatusManagerDesktopTest()
      : identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_) {}

  CoreAccountId SignIn(const std::string& email,
                       bool is_under_advanced_protection) {
    AccountInfo account_info = identity_test_env_.MakeAccountAvailable(email);

    account_info.is_under_advanced_protection = is_under_advanced_protection;
    identity_test_env_.SetPrimaryAccount(account_info.email,
                                         signin::ConsentLevel::kSync);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    return account_info.account_id;
  }

  void MakeOAuthTokenFetchSucceed(const CoreAccountId& account_id,
                                  bool is_under_advanced_protection) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id, "access_token", base::Time::Now() + base::Hours(1),
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

TEST_F(AdvancedProtectionStatusManagerDesktopTest, NotSignedInOnStartUp) {
  base::HistogramTester histograms;

  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // If user's not signed-in. No refresh is required.
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       SignedInLongTimeAgoRefreshFailTransientError) {
  base::HistogramTester histograms;
  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));

  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id,
                          /* is_transient_error = */ true);
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());

  // A retry should be scheduled.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       SignedInLongTimeAgoRefreshFailNonTransientError) {
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id,
                          /* is_transient_error = */ false);
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());

  // No retry should be scheduled.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       SignedInLongTimeAgoNotUnderAP) {
  ASSERT_FALSE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));

  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest, SignedInLongTimeAgoUnderAP) {
  base::HistogramTester histograms;

  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status yet.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ true);

  EXPECT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest, AlreadySignedInAndUnderAP) {
  base::HistogramTester histograms;

  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // under advanced protection.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.IsUnderAdvancedProtection());

  // A refresh is scheduled in the future.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(true, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       AlreadySignedInAndNotUnderAP) {
  base::HistogramTester histograms;

  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // NOT under advanced protection.
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);

  // Incognito profile should share the advanced protection status with the
  // original profile.
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest, StayInAdvancedProtection) {
  base::HistogramTester histograms;

  base::Time last_update = base::Time::Now();
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_update.ToDeltaSinceWindowsEpoch().InMicroseconds());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.IsUnderAdvancedProtection());

  // Simulate gets refresh token.
  aps_manager.OnGetIDToken(account_id, kIdTokenAdvancedProtectionEnabled);
  EXPECT_GT(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
          pref_service_.GetInt64(prefs::kAdvancedProtectionLastRefreshInUs))),
      last_update);
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();

  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(true, 1)));
}

#if !BUILDFLAG(IS_CHROMEOS)
// Not applicable to Chrome OS.
TEST_F(AdvancedProtectionStatusManagerDesktopTest, SignInAndSignOutEvent) {
  base::HistogramTester histograms;

  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.IsUnderAdvancedProtection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));

  SignIn("test@test.com",
         /* is_under_advanced_protection = */ true);
  EXPECT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}
#endif

TEST_F(AdvancedProtectionStatusManagerDesktopTest, AccountRemoval) {
  base::HistogramTester histograms;

  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.IsUnderAdvancedProtection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ false);
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_THAT(histograms.GetAllSamples(kAPEnabledMetric),
              testing::ElementsAre(base::Bucket(false, 1)));

  // Simulates account update.
  identity_test_env_.identity_manager()
      ->GetAccountsMutator()
      ->UpdateAccountInfo(
          account_id,
          /*is_child_account=*/signin::Tribool::kUnknown,
          /*is_under_advanced_protection=*/signin::Tribool::kTrue);
  EXPECT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  identity_test_env_.EnableRemovalOfExtendedAccountInfo();
  identity_test_env_.RemoveRefreshTokenForAccount(account_id);
  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(
      pref_service_.HasPrefPath(prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       AdvancedProtectionDisabledAfterSignin) {
  base::HistogramTester histograms;

  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  // There is no account, so the timer should not run at startup.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);

  // Now that we've signed into Advanced Protection, we should have a scheduled
  // refresh.
  EXPECT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // Skip the 24 hour wait, and try to refresh the token now.
  aps_manager.timer_.FireNow();
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       StartupAfterLongWaitRefreshesImmediately) {
  base::HistogramTester histograms;
  CoreAccountId account_id = SignIn("test@test.com",
                                    /* is_under_advanced_protection = */ true);

  base::Time last_refresh_time = base::Time::Now() - base::Days(1);
  pref_service_.SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refresh_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetUnconsentedPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

// On ChromeOS, there is no unconsented primary account. We can only track the
// primary account.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AdvancedProtectionStatusManagerDesktopTest,
       TracksUnconsentedPrimaryAccount) {
  base::HistogramTester histograms;

  AdvancedProtectionStatusManagerDesktop aps_manager(
      &pref_service_, identity_test_env_.identity_manager(),
      base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.IsUnderAdvancedProtection());
  ASSERT_TRUE(aps_manager.GetUnconsentedPrimaryAccountId().empty());

  // Sign in, but don't set this as the primary account.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@test.com", signin::ConsentLevel::kSignin);
  account_info.is_under_advanced_protection = true;
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_TRUE(aps_manager.IsUnderAdvancedProtection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}
#endif

}  // namespace safe_browsing
