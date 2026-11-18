// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NewTabPageUtilTest : public testing::Test {
 protected:
  NewTabPageUtilTest() {
    TestingProfile::Builder builder;
    builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  Profile* profile() { return profile_.get(); }
  PrefService* prefs() { return profile_->GetPrefs(); }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(NewTabPageUtilTest, FirstNavigationStoresTimeOnly) {
  const base::Time now = base::Time::Now();

  RecordNewTabPageNavigationMetrics(profile(), now);

  histogram_tester_.ExpectTotalCount("NewTabPage.TimeSinceLastNTP", 0);
  histogram_tester_.ExpectUniqueSample("NewTabPage.HasCredentials", false, 1);
  EXPECT_EQ(now, prefs()->GetTime(ntp_prefs::kNtpPrevNavigationTime));
}

TEST_F(NewTabPageUtilTest, RecordsTimeSinceLastNavigation) {
  const base::Time first = base::Time::Now();
  const base::Time second = first + base::Minutes(5);
  const base::Time third = second + base::Days(2);

  RecordNewTabPageNavigationMetrics(profile(), first);
  RecordNewTabPageNavigationMetrics(profile(), second);
  RecordNewTabPageNavigationMetrics(profile(), third);

  // Intervals above the histogram's 1 day maximum land in the overflow bucket.
  histogram_tester_.ExpectTimeBucketCount("NewTabPage.TimeSinceLastNTP",
                                          base::Minutes(5), 1);
  histogram_tester_.ExpectTimeBucketCount("NewTabPage.TimeSinceLastNTP",
                                          base::Days(2), 1);
  histogram_tester_.ExpectTotalCount("NewTabPage.TimeSinceLastNTP", 2);
  EXPECT_EQ(third, prefs()->GetTime(ntp_prefs::kNtpPrevNavigationTime));
}

TEST_F(NewTabPageUtilTest, RecordsHasCredentialsWithCookieAccount) {
  identity_test_env()->SetCookieAccounts(
      {{"user@gmail.com", signin::GetTestGaiaIdForEmail("user@gmail.com")}});

  RecordNewTabPageNavigationMetrics(profile(), base::Time::Now());

  histogram_tester_.ExpectUniqueSample("NewTabPage.HasCredentials", true, 1);
}

TEST_F(NewTabPageUtilTest, IsManagedProfileWithoutPrimaryAccount) {
  EXPECT_FALSE(ntp::IsManagedProfile(profile()));
}

TEST_F(NewTabPageUtilTest, IsManagedProfileWithUnknownHostedDomain) {
  // The hosted domain isn't known until the account info is fetched.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);

  EXPECT_FALSE(ntp::IsManagedProfile(profile()));
}

TEST_F(NewTabPageUtilTest, IsManagedProfileWithConsumerAccount) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(account_info)
          .SetHostedDomain(signin::constants::kNoHostedDomainFound)
          .Build());

  EXPECT_FALSE(ntp::IsManagedProfile(profile()));
}

TEST_F(NewTabPageUtilTest, IsManagedProfileWithManagedAccount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kSkipRefreshTokenCheckInIdentityManager);

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(account_info)
          .SetHostedDomain("example.com")
          .Build());

  EXPECT_TRUE(ntp::IsManagedProfile(profile()));

  // `ntp::IsManagedProfile()` uses `FindExtendedPrimaryAccountInfo()`, so it
  // still sees the hosted domain when no refresh token is loaded (whereas the
  // public `FindExtendedAccountInfo()` returns empty info).
  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
  ASSERT_FALSE(
      identity_test_env()
          ->identity_manager()
          ->HasPrimaryAccountWithRefreshToken(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      identity_test_env()
          ->identity_manager()
          ->FindExtendedAccountInfo(
              identity_test_env()->identity_manager()->GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin))
          .IsEmpty());
  EXPECT_TRUE(ntp::IsManagedProfile(profile()));
}

TEST_F(NewTabPageUtilTest, NullIdentityManagerHandledSafely) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_EQ(nullptr, IdentityManagerFactory::GetForProfile(otr_profile));
  const base::Time now = base::Time::Now();

  EXPECT_FALSE(ntp::IsManagedProfile(otr_profile));
  RecordNewTabPageNavigationMetrics(otr_profile, now);

  histogram_tester_.ExpectUniqueSample("NewTabPage.HasCredentials", false, 1);
  EXPECT_EQ(
      now, otr_profile->GetPrefs()->GetTime(ntp_prefs::kNtpPrevNavigationTime));
}

}  // namespace
