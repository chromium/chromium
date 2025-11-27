// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/supervised_user_log_record.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {
constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kTestEmail1[] = "test1@gmail.com";
constexpr char kTestEmail2[] = "test2@gmail.com";
constexpr char kTestProfile[] = "profile";
constexpr char kTestProfile1[] = "profile1";
constexpr char kTestProfile2[] = "profile2";

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
        test_profile, /*prefs=*/nullptr, base::UTF8ToUTF16(test_profile),
        /*avatar_id=*/0, GetTestingFactories(),
        /*is_supervised_profile=*/is_subject_to_parental_controls,
        /*is_new_profile=*/std::nullopt,
        /*policy_service=*/std::nullopt, /*shared_url_loader_factory=*/nullptr);

    AccountInfo account = signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile), test_email,
        signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    // Tests assume that this account is in Family Link.
    mutator.set_can_fetch_family_member_info(true);
    mutator.set_is_subject_to_parental_controls(
        is_subject_to_parental_controls);
    mutator.set_is_opted_in_to_parental_supervision(
        is_opted_in_to_parental_supervision);
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(profile), account);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (is_subject_to_parental_controls) {
      // Set Family Link `Permissions` switch (and its dependencies) to the
      // default value. Mimics the assignment by the `SupervisedUserPrefStore`.
      supervised_user_test_util::
          SetSupervisedUserExtensionsMayRequestPermissionsPref(profile, true);
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    return profile;
  }

  // Creates the testing factories for the profile. The important part is to
  // inject identity manager related factories and supervised user service
  // factory - which would allow overrides in this test fixture subclasses.
  TestingProfile::TestingFactories GetTestingFactories() {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        SupervisedUserServiceFactory::GetInstance(),
        base::BindOnce(
            &FamilyLinkUserMetricsProviderTest::BuildSupervisedUserService,
            base::Unretained(this)));
    return factories;
  }

  // Default supervised user service, as in production.
  virtual std::unique_ptr<KeyedService> BuildSupervisedUserService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return SupervisedUserServiceFactory::BuildInstanceFor(profile);
  }

  void SetFamilyRole(Profile* profile, kidsmanagement::FamilyRole family_role) {
    profile->GetPrefs()->SetString(prefs::kFamilyLinkUserMemberRole,
                                   FamilyRoleToString(family_role));
  }

  void SetPermissionsToggleForSupervisedUser(Profile* profile, bool enabled) {
    supervised_user_test_util::
        SetSupervisedUserGeolocationEnabledContentSetting(profile, enabled);
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
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), kTestEmail,
      signin::ConsentLevel::kSignin);
  // Does not set account capabilities, default is unknown.

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectTotalCount(kFamilyLinkUserLogSegmentHistogramName,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkPolicy,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kSupervisionEnabledByFamilyLinkUser,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kUnsupervised,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kFamilyLinkUserLogSegmentWebFilterHistogramName,
      WebFilterType::kTryToBlockMatureSites,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kFamilyLinkUserLogSegmentWebFilterHistogramName,
      WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ProfilesWithMixedSupervisedFiltersLoggedAsMixed) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  supervised_user_test_util::SetWebFilterType(profile1,
                                              WebFilterType::kAllowAllSites);
  Profile* profile2 =
      CreateTestingProfile(kTestEmail2, kTestProfile2,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  supervised_user_test_util::SetWebFilterType(profile2,
                                              WebFilterType::kCertainSites);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kFamilyLinkUserLogSegmentWebFilterHistogramName, WebFilterType::kMixed,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       AdultProfileDoesNotHavePermissionLogged) {
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/false,
                       /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      kSitesMayRequestCameraMicLocationHistogramName, ToggleState::kDisabled,
      /*expected_count=*/0);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       SupervisedProfileWithBlockedGeolocationLoggedAsPermissionsDisabled) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetPermissionsToggleForSupervisedUser(profile1, false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSitesMayRequestCameraMicLocationHistogramName, ToggleState::kDisabled,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       SupervisedProfileWithAllowedGeolocationLoggedAsPermissionsEnabled) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetPermissionsToggleForSupervisedUser(profile1, true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSitesMayRequestCameraMicLocationHistogramName, ToggleState::kEnabled,
      /*expected_bucket_count=*/1);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
class FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled
    : public FamilyLinkUserMetricsProviderTest {
 protected:
  FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled() = default;

  void SetExtensionToggleStateForSupervisedUser(Profile* profile,
                                                bool toggle_state) {
    supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
        profile, toggle_state);
  }
};

TEST_F(FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled,
       ProfileWithExtensionToggleStateUnsetLoggedAsDisabled) {
  CreateTestingProfile(kTestEmail1, kTestProfile1,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSkipParentApprovalToInstallExtensionsHistogramName,
      ToggleState::kDisabled,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled,
       ProfileWithExtensionToggleStateOffLoggedAsDisabled) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetExtensionToggleStateForSupervisedUser(profile1, false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSkipParentApprovalToInstallExtensionsHistogramName,
      ToggleState::kDisabled,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled,
       ProfileWithExtensionToggleStateOnLoggedAsEnabled) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetExtensionToggleStateForSupervisedUser(profile1, true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSkipParentApprovalToInstallExtensionsHistogramName,
      ToggleState::kEnabled,
      /*expected_bucket_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTestWithExtensionsPermissionsEnabled,
       ProfilesWithMixedExtensionToggleStateLoggedAsMixed) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetExtensionToggleStateForSupervisedUser(profile1, true);

  Profile* profile2 =
      CreateTestingProfile(kTestEmail2, kTestProfile2,
                           /*is_subject_to_parental_controls=*/true,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetExtensionToggleStateForSupervisedUser(profile2, false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectUniqueSample(
      kSkipParentApprovalToInstallExtensionsHistogramName, ToggleState::kMixed,
      /*expected_bucket_count=*/1);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(FamilyLinkUserMetricsProviderTest,
       NoProfilesAddedShouldNotLogHistogram) {
  // Add no profiles
  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kMixedProfile,
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
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kUnsupervised,
      /*expected_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest, ParentProfileLoggedAsParent) {
  Profile* profile =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile, kidsmanagement::PARENT);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(kFamilyLinkUserLogSegmentHistogramName,
                                     SupervisedUserLogRecord::Segment::kParent,
                                     /*expected_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest, FamilyManagerProfileLoggedAsParent) {
  Profile* profile =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile, kidsmanagement::HEAD_OF_HOUSEHOLD);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(kFamilyLinkUserLogSegmentHistogramName,
                                     SupervisedUserLogRecord::Segment::kParent,
                                     /*expected_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest, ParentAndChildProfileLoggedAsMixed) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile1, kidsmanagement::HEAD_OF_HOUSEHOLD);

  CreateTestingProfile(kTestEmail2, kTestProfile2,
                       /*is_subject_to_parental_controls=*/true,
                       /*is_opted_in_to_parental_supervision=*/false);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kMixedProfile,
      /*expected_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest, TwoParentProfilesLoggedAsParent) {
  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile1, kidsmanagement::HEAD_OF_HOUSEHOLD);

  Profile* profile2 =
      CreateTestingProfile(kTestEmail2, kTestProfile2,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile2, kidsmanagement::PARENT);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(kFamilyLinkUserLogSegmentHistogramName,
                                     SupervisedUserLogRecord::Segment::kParent,
                                     /*expected_count=*/1);
}

TEST_F(FamilyLinkUserMetricsProviderTest,
       ParentAndSignedOutProfilesLoggedAsParent) {
  test_profile_manager()->CreateTestingProfile(
      kTestProfile, IdentityTestEnvironmentProfileAdaptor::
                        GetIdentityTestEnvironmentFactories());

  Profile* profile1 =
      CreateTestingProfile(kTestEmail1, kTestProfile1,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile1, kidsmanagement::HEAD_OF_HOUSEHOLD);

  Profile* profile2 =
      CreateTestingProfile(kTestEmail2, kTestProfile2,
                           /*is_subject_to_parental_controls=*/false,
                           /*is_opted_in_to_parental_supervision=*/false);
  SetFamilyRole(profile2, kidsmanagement::PARENT);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();
  histogram_tester.ExpectBucketCount(kFamilyLinkUserLogSegmentHistogramName,
                                     SupervisedUserLogRecord::Segment::kParent,
                                     /*expected_count=*/1);
}

#if BUILDFLAG(IS_ANDROID)
struct ContentFiltersTestCase {
  std::size_t profile_count;
  std::string test_name;
};

// Test fixture for verifying that the content filters are correctly
// reflected in the metrics. Content filters are mutually exclusive with
// Family-Link supervision and cannot be applied to these profiles.
class FamilyLinkUserMetricsProviderWithContentFiltersTest
    : public FamilyLinkUserMetricsProviderTest,
      public testing::WithParamInterface<ContentFiltersTestCase> {
 protected:
  void CreateProfiles(std::size_t count) {
    CHECK_GE(email_addresses_.size(), count) << "Not enough email addresses";
    CHECK_GE(profile_names_.size(), count) << "Not enough profile names";

    for (std::size_t i = 0; i < count; ++i) {
      Profile* unsupervised_profile = CreateUnsupervisedTestingProfile(
          email_addresses_[i], profile_names_[i]);

      // Services are lazily created, so we need to access them to force their
      // creation. That'll trigger the ::BuildSupervisedUserService factory,
      // which will bind fake content filters observers to this text fixture
      // instance.
      CHECK(SupervisedUserServiceFactory::GetInstance()->GetForProfile(
          unsupervised_profile));
    }
  }

  Profile* CreateUnsupervisedTestingProfile(const std::string& email,
                                            const std::string& profile_name) {
    // Content filters are not supported for family link supervised profiles.
    return CreateTestingProfile(email, profile_name,
                                /*is_subject_to_parental_controls=*/false,
                                /*is_opted_in_to_parental_supervision=*/false);
  }

  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls) {
    std::unique_ptr<FakeContentFiltersObserverBridge> bridge =
        std::make_unique<FakeContentFiltersObserverBridge>(
            setting_name, on_enabled, on_disabled,
            is_subject_to_parental_controls);
    if (setting_name == kBrowserContentFiltersSettingName) {
      browser_content_filters_observers_.push_back(bridge.get());
    } else if (setting_name == kSearchContentFiltersSettingName) {
      search_content_filters_observers_.push_back(bridge.get());
    }
    return bridge;
  }

  // Builds the `SupervisedUserService` and captures the content observers onto
  // this text fixture instance. Binding memory managed by foreign object (the
  // service) to raw_ptr is fine, because raw_ptr handles will be destroyed
  // before profile manager, that transitively owns the content observers.
  std::unique_ptr<KeyedService> BuildSupervisedUserService(
      content::BrowserContext* browser_context) override {
    Profile* profile = Profile::FromBrowserContext(browser_context);

    std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);

    return std::make_unique<SupervisedUserService>(
        IdentityManagerFactory::GetForProfile(profile),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        *profile->GetPrefs(),
        *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()),
        SupervisedUserContentFiltersServiceFactory::GetInstance()->GetForKey(
            profile->GetProfileKey()),
        SyncServiceFactory::GetInstance()->GetForProfile(profile),
        std::make_unique<SupervisedUserURLFilter>(
            *profile->GetPrefs(), std::make_unique<FakeURLFilterDelegate>(),
            std::make_unique<KidsChromeManagementURLCheckerClient>(
                IdentityManagerFactory::GetForProfile(profile),
                profile->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess(),
                *profile->GetPrefs(), platform_delegate->GetCountryCode(),
                platform_delegate->GetChannel())),
        std::make_unique<SupervisedUserServicePlatformDelegate>(*profile),
        base::BindRepeating(
            &FamilyLinkUserMetricsProviderWithContentFiltersTest::CreateBridge,
            base::Unretained(this)));
  }

  // Enables or disables the browser content filters for all profiles.
  void SetBrowserContentFilters(bool enabled) {
    for (FakeContentFiltersObserverBridge* observer :
         browser_content_filters_observers_) {
      observer->SetEnabled(enabled);
    }
  }

  // Enables or disables the search content filters for all profiles.
  void SetSearchContentFilters(bool enabled) {
    for (FakeContentFiltersObserverBridge* observer :
         search_content_filters_observers_) {
      observer->SetEnabled(enabled);
    }
  }

 private:
  // Required to propagate the device content filters to the supervised user
  // service. FakeContentFiltersObserverBridge which is in action here only
  // avoids creating the java bridge class, but uses prod notification patterns.
  base::test::ScopedFeatureList scoped_feature_list_{
      kPropagateDeviceContentFiltersToSupervisedUser};
  std::vector<std::string> email_addresses_{kTestEmail, kTestEmail1};
  std::vector<std::string> profile_names_{kTestProfile, kTestProfile1};

  // These are internal components of the `SupervisedUserService` and are used
  // to simulate changes to the android content filters. Both are owned by the
  // `SupervisedUserService`. The reason for they're held in vectors is that
  // content filters are applied at device level, for all profiles at once.
  std::vector<raw_ptr<FakeContentFiltersObserverBridge>>
      browser_content_filters_observers_;
  std::vector<raw_ptr<FakeContentFiltersObserverBridge>>
      search_content_filters_observers_;
};

TEST_P(FamilyLinkUserMetricsProviderWithContentFiltersTest,
       AllFiltersDisabled) {
  CreateProfiles(GetParam().profile_count);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kUnsupervised,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      kFamilyLinkUserLogSegmentWebFilterHistogramName,
      /*expected_count=*/0);
}

TEST_P(FamilyLinkUserMetricsProviderWithContentFiltersTest,
       SearchFilterEnabled) {
  CreateProfiles(GetParam().profile_count);
  SetSearchContentFilters(true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentWebFilterHistogramName, WebFilterType::kDisabled,
      /*expected_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderWithContentFiltersTest,
       ContentFiltersEnabled) {
  CreateProfiles(GetParam().profile_count);
  SetBrowserContentFilters(true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kFamilyLinkUserLogSegmentWebFilterHistogramName,
      WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

TEST_P(FamilyLinkUserMetricsProviderWithContentFiltersTest, AllFiltersEnabled) {
  CreateProfiles(GetParam().profile_count);
  SetBrowserContentFilters(true);
  SetSearchContentFilters(true);

  base::HistogramTester histogram_tester;
  metrics_provider()->OnDidCreateMetricsLog();

  histogram_tester.ExpectBucketCount(
      kFamilyLinkUserLogSegmentHistogramName,
      SupervisedUserLogRecord::Segment::kSupervisionEnabledLocally,
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kFamilyLinkUserLogSegmentWebFilterHistogramName,
      WebFilterType::kTryToBlockMatureSites,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FamilyLinkUserMetricsProviderWithContentFiltersTest,
    testing::ValuesIn<ContentFiltersTestCase>({
        {1, "SingleProfile"},
        {2, "MultipleProfiles"},
    }),
    [](const testing::TestParamInfo<ContentFiltersTestCase>& info) {
      return info.param.test_name;
    });
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace supervised_user
