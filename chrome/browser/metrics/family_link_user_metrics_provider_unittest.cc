// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kTestEmail1[] = "test1@gmail.com";
constexpr char kTestEmail2[] = "test2@gmail.com";
constexpr char kTestProfile[] = "profile";
constexpr char kTestProfile1[] = "profile1";
constexpr char kTestProfile2[] = "profile2";

}  // namespace

class FamilyLinkUserMetricsProviderTestLegacy : public testing::Test {
 protected:
  FamilyLinkUserMetricsProviderTestLegacy() = default;

  void SetUp() override {
    EnableAccountCapabilitiesFetches(identity_manager());
    feature_list_.InitAndDisableFeature(
        kExtendFamilyLinkUserLogSegmentToAllPlatforms);
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

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  FamilyLinkUserMetricsProvider metrics_provider_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, UserWithUnknownCapabilities) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Does not set account capabilities, default is unknown.
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      /*expected_count=*/0);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, AdultUser) {
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
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, UserWithOptionalSupervision) {
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
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, UserWithRequiredSupervision) {
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
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy,
       MetricsProviderInitAfterPrimaryAccountAdded) {
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  // Identity manager observer set after primary account is made available.
  metrics_provider()->IdentityManagerCreated(identity_manager());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      /*expected_count=*/0);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy,
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
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, SetChildAsPrimaryAccount) {
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
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(), 0);

  identity_test_env()->SetPrimaryAccount(kTestEmail,
                                         signin::ConsentLevel::kSignin);

  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestLegacy, ClearLogOnUserSignout) {
  metrics_provider()->IdentityManagerCreated(identity_manager());
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  mutator.set_can_stop_parental_supervision(false);
  signin::UpdateAccountInfoForAccount(identity_manager(), account);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);

  identity_test_env()->ClearPrimaryAccount();
  metrics_provider()->OnDidCreateMetricsLog();

  // The histogram should stay the same since the user has signed out.
  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

class FamilyLinkUserMetricsProviderTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  FamilyLinkUserMetricsProviderTest()
      : test_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(test_profile_manager_.SetUp());
    metrics_provider()->skip_active_browser_count_for_unittesting_ = true;

    bool enable_extended_family_link_user_log_segment = GetParam();
    if (enable_extended_family_link_user_log_segment) {
      feature_list_.InitWithFeatures(
          {kExtendFamilyLinkUserLogSegmentToAllPlatforms}, {});
    } else {
      feature_list_.InitWithFeatures(
          {kExtendFamilyLinkUserLogSegmentToAllPlatforms}, {});
    }
  }

  void TearDown() override { test_profile_manager_.DeleteAllTestingProfiles(); }

  FamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  TestingProfileManager* test_profile_manager() {
    return &test_profile_manager_;
  }

  void CreateTestingProfile(const std::string& test_email,
                            const std::string& test_profile,
                            bool is_subject_to_parental_controls,
                            bool can_stop_parental_supervision) {
    Profile* profile = test_profile_manager()->CreateTestingProfile(
        test_profile, IdentityTestEnvironmentProfileAdaptor::
                          GetIdentityTestEnvironmentFactories());

    EnableAccountCapabilitiesFetches(
        IdentityManagerFactory::GetForProfile(profile));
    AccountInfo account = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile), test_email,
        signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_can_stop_parental_supervision(can_stop_parental_supervision);
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(profile), account);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FamilyLinkUserMetricsProvider metrics_provider_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager test_profile_manager_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FamilyLinkUserMetricsProviderTest,
                         testing::Bool());

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfileWithUnknownCapabilitiesDoesNotOutputHistogram) {
  Profile* profile = test_profile_manager()->CreateTestingProfile(
      kTestProfile, IdentityTestEnvironmentProfileAdaptor::
                        GetIdentityTestEnvironmentFactories());
  EnableAccountCapabilitiesFetches(
      IdentityManagerFactory::GetForProfile(profile));
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), kTestEmail,
      signin::ConsentLevel::kSignin);
  // Does not set account capabilities, default is unknown.

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectTotalCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      /*expected_count=*/0);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfileWithRequiredSupervisionLoggedAsSupervisionEnabledByPolicy) {
  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfileWithOptionalSupervisionLoggedSupervisionEnabledByUser) {
  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfileWithAdultUserLoggedAsUnsupervised) {
  // Adult profile
  CreateTestingProfile(kTestEmail, kTestProfile,
                       /*is_subject_to_parental_controls=*/false,
                       /*can_stop_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedUsersLoggedAsMixedProfile) {
  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/false);
  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kMixedProfile,
      /*expected_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedAndAdultUsersLoggedAsMixedProfile) {
  // Adult profile
  CreateTestingProfile(kTestEmail, kTestProfile,
                       /*is_subject_to_parental_controls=*/false,
                       /*can_stop_parental_supervision=*/false);

  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/false);

  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*can_stop_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kMixedProfile,
      /*expected_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderTest,
       NoProfilesAddedShouldNotLogHistogram) {
  base::test::ScopedFeatureList feature{
      kExtendFamilyLinkUserLogSegmentToAllPlatforms};

  // Add no profiles
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      FamilyLinkUserMetricsProvider::GetHistogramNameForTesting(),
      FamilyLinkUserMetricsProvider::LogSegment::kMixedProfile,
      /*expected_count=*/0);
}
