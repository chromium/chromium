// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
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
class FamilyLinkUserMetricsProviderTest : public testing::Test {
 protected:
  FamilyLinkUserMetricsProviderTest()
      : test_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(test_profile_manager_.SetUp());
    metrics_provider()->skip_active_browser_count_for_unittesting_ = true;
  }

  void TearDown() override { test_profile_manager_.DeleteAllTestingProfiles(); }

  FamilyLinkUserMetricsProvider* metrics_provider() {
    return &metrics_provider_;
  }

  TestingProfileManager* test_profile_manager() {
    return &test_profile_manager_;
  }

  Profile* CreateTestingProfile(const std::string& test_email,
                                const std::string& test_profile,
                                bool is_subject_to_parental_controls,
                                bool is_opted_in_to_parental_supervision) {
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
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(profile), account);

    if (is_subject_to_parental_controls) {
      supervised_user::EnableParentalControls(*profile->GetPrefs());
    }
    return profile;
  }

  void RestrictAllSitesForSupervisedUser(Profile* profile) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user_service->GetURLFilter()->SetDefaultFilteringBehavior(
        supervised_user::FilteringBehavior::kBlock);
  }

  void AllowUnsafeSitesForSupervisedUser(Profile* profile) {
    profile->GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, false);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FamilyLinkUserMetricsProvider metrics_provider_;
  TestingProfileManager test_profile_manager_;
};

TEST_F(FamilyLinkUserMetricsProviderTest,
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
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      /*expected_count=*/0);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ProfileWithRequiredSupervisionLoggedAsSupervisionEnabledByPolicy) {
  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ProfileWithOptionalSupervisionLoggedSupervisionEnabledByUser) {
  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::
          kSupervisionEnabledByUser,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ProfileWithAdultUserLoggedAsUnsupervised) {
  // Adult profile
  CreateTestingProfile(kTestEmail, kTestProfile,
                       /*is_subject_to_parental_controls=*/false,
                       /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kUnsupervised,
      /*expected_bucket_count=*/1);
}

TEST_F(
    FamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);
  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(
    FamilyLinkUserMetricsProviderTest,
    ProfilesWithMixedSupervisedAndAdultUsersLoggedAsMixedProfileWithDefaultWebFilter) {
  // Adult profile
  CreateTestingProfile(kTestEmail, kTestProfile,
                       /*is_subject_to_parental_controls=*/false,
                       /*is_opted_in_to_parental_supervision=*/false);

  // Profile with supervision set by user
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  // Profile with supervision set by policy
  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedFiltersLoggedAsMixed) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  AllowUnsafeSitesForSupervisedUser(profile1);

  Profile* profile2 =
      CreateTestingProfile(kTestEmail2, kTestProfile2,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  RestrictAllSitesForSupervisedUser(profile2);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      supervised_user::kFamilyLinkUserLogSegmentWebFilterHistogramName,
      supervised_user::WebFilterType::kMixed,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       NoProfilesAddedShouldNotLogHistogram) {
  // Add no profiles
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/0);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       SignedOutProfileLoggedAsUnsupervised) {
  test_profile_manager()->CreateTestingProfile(
      kTestProfile, IdentityTestEnvironmentProfileAdaptor::
                        GetIdentityTestEnvironmentFactories());

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      supervised_user::kFamilyLinkUserLogSegmentHistogramName,
      supervised_user::FamilyLinkUserLogRecord::Segment::kUnsupervised,
      /*expected_count=*/1);
}
