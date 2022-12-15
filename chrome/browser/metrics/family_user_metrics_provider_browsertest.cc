// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_user_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

constexpr char kSecondaryEDUEmail[] = "testuser1@managedchrome.com";

// Returns the user type for the primary test account for logging in.
ash::LoggedInUserMixin::LogInType GetPrimaryLogInType(
    FamilyUserMetricsProvider::FamilyUserLogSegment log_segment) {
  switch (log_segment) {
    case FamilyUserMetricsProvider::FamilyUserLogSegment::kSupervisedUser:
    case FamilyUserMetricsProvider::FamilyUserLogSegment::kSupervisedStudent:
      return ash::LoggedInUserMixin::LogInType::kChild;
    case FamilyUserMetricsProvider::FamilyUserLogSegment::kStudentAtHome:
    case FamilyUserMetricsProvider::FamilyUserLogSegment::kRegularUser:
    case FamilyUserMetricsProvider::FamilyUserLogSegment::kOther:
      return ash::LoggedInUserMixin::LogInType::kRegular;
  }
}

// Returns the account id for the primary test account for logging in.
absl::optional<AccountId> GetPrimaryAccountId(
    FamilyUserMetricsProvider::FamilyUserLogSegment log_segment) {
  if (log_segment ==
      FamilyUserMetricsProvider::FamilyUserLogSegment::kStudentAtHome) {
    // To distinguish K-12 EDU users from Enterprise users in ChromeOS, we use a
    // PolicyData field. Fetching policy is skipped for obviously consumer
    // users, who have an @gmail.com e-mail, for example (see comments in
    // fake_gaia_mixin.h). Since we need policies for this test, we must use an
    // e-mail address that has an enterprise domain. Of all the user categories,
    // kStudentAtHome is the only one with an enterprise managed primary
    // account.
    return AccountId::FromUserEmailGaiaId(
        FakeGaiaMixin::kEnterpriseUser1, FakeGaiaMixin::kEnterpriseUser1GaiaId);
  }
  // Use the default FakeGaiaMixin::kFakeUserEmail consumer test account id.
  return absl::nullopt;
}

void ProvideHistograms(bool should_emit_histograms_earlier) {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  metrics::ChromeUserMetricsExtension uma_proto;
  if (!should_emit_histograms_earlier) {
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideCurrentSessionData(&uma_proto);
  } else {
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->OnDidCreateMetricsLog();
  }
}

}  // namespace

struct FamilyUserMetricsProviderTestParams {
  FamilyUserMetricsProvider::FamilyUserLogSegment family_user_log_segment;
  bool emit_histograms_earlier;
};

class FamilyUserMetricsProviderTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<FamilyUserMetricsProviderTestParams> {
 public:
  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    if (GetFamilyUserLogSegment() ==
        FamilyUserMetricsProvider::FamilyUserLogSegment::kStudentAtHome) {
      logged_in_user_mixin_.GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_data()
          ->set_metrics_log_segment(enterprise_management::PolicyData::K12);
    }
  }

 protected:
  FamilyUserMetricsProvider::FamilyUserLogSegment GetFamilyUserLogSegment()
      const {
    return GetParam().family_user_log_segment;
  }

  bool ShouldEmitHistogramsEarlier() const {
    return GetParam().emit_histograms_earlier;
  }

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, GetPrimaryLogInType(GetFamilyUserLogSegment()),
      embedded_test_server(), this,
      /*should_launch_browser=*/true,
      GetPrimaryAccountId(GetFamilyUserLogSegment()),
      /*include_initial_user=*/true,
      // Don't use EmbeddedPolicyTestServer because it does not support
      // customizing PolicyData.
      // TODO(crbug/1112885): Use EmbeddedPolicyTestServer when this is fixed.
      /*use_embedded_policy_server=*/false};

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(FamilyUserMetricsProviderTest, UserCategory) {
  base::HistogramTester histogram_tester;
  // Simulate calling ProvideHistograms() prior to logging in. This call should
  // return prematurely.
  ProvideHistograms(ShouldEmitHistogramsEarlier());

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      FamilyUserMetricsProvider::
          GetFamilyUserLogSegmentHistogramNameForTesting(),
      0);
  histogram_tester.ExpectTotalCount(
      FamilyUserMetricsProvider::
          GetNumSecondaryAccountsHistogramNameForTesting(),
      0);

  logged_in_user_mixin_.LogInUser();
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(browser()->profile()));

  if (GetFamilyUserLogSegment() ==
      FamilyUserMetricsProvider::FamilyUserLogSegment::kSupervisedStudent) {
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

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms(ShouldEmitHistogramsEarlier());

  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::
          GetFamilyUserLogSegmentHistogramNameForTesting(),
      GetFamilyUserLogSegment(), /*expected_count=*/1);

  int expected_num_secondary_accounts =
      GetFamilyUserLogSegment() == FamilyUserMetricsProvider::
                                       FamilyUserLogSegment::kSupervisedStudent
          ? 1
          : 0;
  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::
          GetNumSecondaryAccountsHistogramNameForTesting(),
      expected_num_secondary_accounts, /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyUserMetricsProviderTest,
    testing::Values(
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment = FamilyUserMetricsProvider::
                FamilyUserLogSegment::kSupervisedUser,
            .emit_histograms_earlier = true},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment = FamilyUserMetricsProvider::
                FamilyUserLogSegment::kSupervisedStudent,
            .emit_histograms_earlier = true},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment =
                FamilyUserMetricsProvider::FamilyUserLogSegment::kStudentAtHome,
            .emit_histograms_earlier = true},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment =
                FamilyUserMetricsProvider::FamilyUserLogSegment::kRegularUser,
            .emit_histograms_earlier = true},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment = FamilyUserMetricsProvider::
                FamilyUserLogSegment::kSupervisedUser,
            .emit_histograms_earlier = false},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment = FamilyUserMetricsProvider::
                FamilyUserLogSegment::kSupervisedStudent,
            .emit_histograms_earlier = false},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment =
                FamilyUserMetricsProvider::FamilyUserLogSegment::kStudentAtHome,
            .emit_histograms_earlier = false},
        FamilyUserMetricsProviderTestParams{
            .family_user_log_segment =
                FamilyUserMetricsProvider::FamilyUserLogSegment::kRegularUser,
            .emit_histograms_earlier = false}));

class FamilyUserMetricsProviderBaseTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }

    MixinBasedInProcessBrowserTest::SetUp();
  }

  bool ShouldEmitHistogramsEarlier() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class FamilyUserMetricsProviderGuestModeTest
    : public FamilyUserMetricsProviderBaseTest {
 private:
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};
};

INSTANTIATE_TEST_SUITE_P(All,
                         FamilyUserMetricsProviderGuestModeTest,
                         testing::Bool());

// Prevents a regression to crbug/1137352. Also tests secondary account metrics
// not reported in guest mode.
IN_PROC_BROWSER_TEST_P(FamilyUserMetricsProviderGuestModeTest,
                       NoCrashInGuestMode) {
  base::HistogramTester histogram_tester;

  ProvideHistograms(ShouldEmitHistogramsEarlier());

  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::
          GetFamilyUserLogSegmentHistogramNameForTesting(),
      FamilyUserMetricsProvider::FamilyUserLogSegment::kOther,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      FamilyUserMetricsProvider::
          GetNumSecondaryAccountsHistogramNameForTesting(),
      /*expected_count=*/0);
}

class FamilyUserMetricsProviderEphemeralUserTest
    : public FamilyUserMetricsProviderBaseTest {
 protected:
  // MixinBasedInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    std::unique_ptr<ash::ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);

    device_policy_update.reset();

    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceLoginLogout, false);
  }

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
    signin::WaitForRefreshTokensLoaded(
        IdentityManagerFactory::GetForProfile(browser()->profile()));
  }

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this};

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FamilyUserMetricsProviderEphemeralUserTest,
                         testing::Bool());

// Tests that regular ephemeral users report 0 for number of secondary accounts.
IN_PROC_BROWSER_TEST_P(FamilyUserMetricsProviderEphemeralUserTest,
                       EphemeralUserZeroSecondaryAccounts) {
  base::HistogramTester histogram_tester;

  ProvideHistograms(ShouldEmitHistogramsEarlier());

  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::
          GetFamilyUserLogSegmentHistogramNameForTesting(),
      FamilyUserMetricsProvider::FamilyUserLogSegment::kRegularUser,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserMetricsProvider::
          GetNumSecondaryAccountsHistogramNameForTesting(),
      /*sample=*/0,
      /*expected_count=*/1);
}
