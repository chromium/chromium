// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kSecondaryEDUEmail[] = "testuser1@managedchrome.com";

// Returns the user type for the primary test account for logging in.
chromeos::LoggedInUserMixin::LogInType GetPrimaryLogInType(
    FamilyUserMetricsProvider::LogSegment log_segment) {
  switch (log_segment) {
    case FamilyUserMetricsProvider::LogSegment::kSupervisedUser:
    case FamilyUserMetricsProvider::LogSegment::kSupervisedStudent:
      return chromeos::LoggedInUserMixin::LogInType::kChild;
    case FamilyUserMetricsProvider::LogSegment::kStudentAtHome:
    case FamilyUserMetricsProvider::LogSegment::kOther:
      return chromeos::LoggedInUserMixin::LogInType::kRegular;
  }
}

// Returns the account id for the primary test account for logging in.
base::Optional<AccountId> GetPrimaryAccountId(
    FamilyUserMetricsProvider::LogSegment log_segment) {
  if (log_segment == FamilyUserMetricsProvider::LogSegment::kStudentAtHome) {
    // To distinguish K-12 EDU users from Enterprise users in ChromeOS, we use a
    // PolicyData field. Fetching policy is skipped for obviously consumer
    // users, who have an @gmail.com e-mail, for example (see comments in
    // fake_gaia_mixin.h). Since we need policies for this test, we must use an
    // e-mail address that has an enterprise domain. Of all the user categories,
    // kStudentAtHome is the only one with an enterprise managed primary
    // account.
    return AccountId::FromUserEmailGaiaId(
        chromeos::FakeGaiaMixin::kEnterpriseUser1,
        chromeos::FakeGaiaMixin::kEnterpriseUser1GaiaId);
  }
  // Use the default FakeGaiaMixin::kFakeUserEmail consumer test account id.
  return base::nullopt;
}

}  // namespace

class FamilyUserMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          FamilyUserMetricsProvider::LogSegment> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    const FamilyUserMetricsProvider::LogSegment log_segment = GetParam();
    if (log_segment == FamilyUserMetricsProvider::LogSegment::kStudentAtHome) {
      logged_in_user_mixin_.GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_data()
          ->set_metrics_log_segment(enterprise_management::PolicyData::K12);
    }
  }

 protected:
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, GetPrimaryLogInType(GetParam()), embedded_test_server(),
      this,
      /*should_launch_browser=*/true, GetPrimaryAccountId(GetParam()),
      /*include_initial_user=*/true,
      // Don't use LocalPolicyTestServer because it does not support customizing
      // PolicyData.
      // TODO(crbug/1112885): Use LocalPolicyTestServer when this is fixed.
      /*use_local_policy_server=*/false};
};

IN_PROC_BROWSER_TEST_P(FamilyUserMetricsProviderTest, DISABLED_UserCategory) {
  base::HistogramTester histogram_tester;
  FamilyUserMetricsProvider provider;
  // Simulate calling ProvideCurrentSessionData() prior to logging in.
  // This call should return prematurely.
  provider.ProvideCurrentSessionData(/*uma_proto_unused=*/nullptr);

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      FamilyUserMetricsProvider::kFamilyUserLogSegmentHistogramName, 0);

  logged_in_user_mixin_.LogInUser();

  const FamilyUserMetricsProvider::LogSegment log_segment = GetParam();
  if (log_segment ==
      FamilyUserMetricsProvider::LogSegment::kSupervisedStudent) {
    // Add a secondary EDU account.
    Profile* profile = browser()->profile();
    ASSERT_TRUE(profile);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    AccountInfo account_info =
        signin::MakeAccountAvailable(identity_manager, kSecondaryEDUEmail);
    EXPECT_TRUE(
        identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  }

  // Simulate calling ProvideCurrentSessionData() after logging in.
  provider.ProvideCurrentSessionData(/*uma_proto_unused=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::kFamilyUserLogSegmentHistogramName,
      log_segment, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserMetricsProviderTest,
    testing::Values(FamilyUserMetricsProvider::LogSegment::kSupervisedUser,
                    FamilyUserMetricsProvider::LogSegment::kSupervisedStudent,
                    FamilyUserMetricsProvider::LogSegment::kStudentAtHome,
                    FamilyUserMetricsProvider::LogSegment::kOther));
