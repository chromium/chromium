// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";
}  // namespace

class FamilyLinkUserMetricsProviderTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  FamilyLinkUserMetricsProviderTest() = default;

  void SetUp() override {
    EnableAccountCapabilitiesFetches(identity_manager());

    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }
  }

  void TearDown() override {
    metrics_provider()->OnIdentityManagerShutdown(identity_manager());
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  FamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  void ProvideHistograms() {
    if (!ShouldEmitHistogramsEarlier()) {
      metrics::ChromeUserMetricsExtension uma_proto;
      metrics_provider()->ProvideCurrentSessionData(&uma_proto);
    } else {
      metrics_provider()->OnDidCreateMetricsLog();
    }
  }

  bool ShouldEmitHistogramsEarlier() const { return GetParam(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  FamilyLinkUserMetricsProvider metrics_provider_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FamilyLinkUserMetricsProviderTest,
                         testing::Bool());

TEST_P(FamilyLinkUserMetricsProviderTest, UserWithUnknownCapabilities) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Does not set account capabilities, default is unknown.
  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      /*count=*/0);
}

TEST_P(FamilyLinkUserMetricsProviderTest, AdultUser) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_can_stop_parental_supervision(false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  AccountInfo updated_account =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account.gaia);
  ASSERT_EQ(signin::Tribool::kFalse,
            updated_account.capabilities.is_subject_to_parental_controls());

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest, UserWithOptionalSupervision) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_can_stop_parental_supervision(true);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  AccountInfo updated_account =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account.gaia);
  ASSERT_EQ(signin::Tribool::kTrue,
            updated_account.capabilities.is_subject_to_parental_controls());
  ASSERT_EQ(signin::Tribool::kTrue,
            updated_account.capabilities.can_stop_parental_supervision());

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest, UserWithRequiredSupervision) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_can_stop_parental_supervision(false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  AccountInfo updated_account =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account.gaia);
  ASSERT_EQ(signin::Tribool::kTrue,
            updated_account.capabilities.is_subject_to_parental_controls());
  ASSERT_EQ(signin::Tribool::kFalse,
            updated_account.capabilities.can_stop_parental_supervision());

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       MetricsProviderInitAfterPrimaryAccountAdded) {
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Identity manager observer set after primary account is made available.
  metrics_provider()->IdentityManagerCreated(identity_manager());

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      /*count=*/0);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       MetricsProviderInitAfterPrimaryAccountWithCapabilitiesAdded) {
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_can_stop_parental_supervision(false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  // Identity manager observer set after primary account is made available.
  metrics_provider()->IdentityManagerCreated(identity_manager());

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest, SetChildAsPrimaryAccount) {
  // Add child account to the device as a secondary account. This allows us to
  // simulate a cached account state once we set the account as primary.
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_can_stop_parental_supervision(false);
  identity_test_env()->UpdateAccountInfoForAccount(account);

  // Identity manager observer set after account capabilities are updated.
  metrics_provider()->IdentityManagerCreated(identity_manager());

  // There is no primary account so the account metrics will not be recorded.
  // This simulates a signed-out client who signs back in to a previously loaded
  // child account.
  base::HistogramTester histogram_tester;
  ProvideHistograms();
  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(), 0);

  identity_test_env()->SetPrimaryAccount(kTestEmail,
                                         signin::ConsentLevel::kSignin);

  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest, ClearLogOnUserSignout) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_can_stop_parental_supervision(false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  base::HistogramTester histogram_tester;
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);

  identity_test_env()->ClearPrimaryAccount();
  ProvideHistograms();

  // The histogram should stay the same since the user has signed out.
  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}
