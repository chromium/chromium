// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::test::FeatureRef;

namespace glic {
namespace {

class GlicEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    // Enable kGlic and kTabstripComboButton by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kTabstripComboButton,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test
TEST_F(GlicEnablingTest, GlicFeatureEnabledTest) {
  EXPECT_EQ(GlicEnabling::IsEnabledByFlags(), true);
}

TEST_F(GlicEnablingTest, GlicFeatureNotEnabledTest) {
  // Turn feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kGlic});
  EXPECT_EQ(GlicEnabling::IsEnabledByFlags(), false);
}

TEST_F(GlicEnablingTest, TabStripComboButtonFeatureNotEnabledTest) {
  // Turn tab strip combo button feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kTabstripComboButton});
  EXPECT_EQ(GlicEnabling::IsEnabledByFlags(), false);
}

// Test for `glic::GlicEnabling::IsProfileEligible`.
class GlicEnablingProfileEligibilityTest : public testing::Test {
 public:
  GlicEnablingProfileEligibilityTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
            features::kTabstripComboButton,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});
  }
  ~GlicEnablingProfileEligibilityTest() override = default;

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager_->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();

    profile_ = nullptr;
    testing_profile_manager_.reset();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 protected:
  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(GlicEnablingProfileEligibilityTest, Eligible) {
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
}

class GlicEnablingProfileReadyStateTestBase
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingProfileReadyStateTestBase() = default;

  void SetUp() override {
    GlicEnablingProfileEligibilityTest::SetUp();
    // Ensure the profile is Enabled by default.
    // Disable rollout check and user status check complexities for these tests.
    // We already have kGlic enabled from the base class.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicRollout},
        /*disabled_features=*/{features::kGlicUserStatusCheck});

    // Make sure we have a primary account so we don't fail the "capable" check.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicEnablingTrustFirstOnboardingTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicTrustFirstOnboarding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicEnablingStandardFreTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicTrustFirstOnboarding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicEnablingTrustFirstOnboardingTest, NotConsented_ReturnsReady) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

TEST_F(GlicEnablingStandardFreTest, NotConsented_ReturnsIneligible) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}

class GlicEnablingAnyFreModeTest : public GlicEnablingProfileReadyStateTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kGlicTrustFirstOnboarding);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kGlicTrustFirstOnboarding);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GlicEnablingAnyFreModeTest, Consented_ReturnsReady) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_P(GlicEnablingAnyFreModeTest, NotSignedIn_ReturnsIneligible) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  // Simulate "Not signed in" by removing the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_P(GlicEnablingAnyFreModeTest,
       IsEnabledAndConsentForProfile_NotConsented_ReturnsFalse) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

TEST_P(GlicEnablingAnyFreModeTest,
       IsEnabledAndConsentForProfile_Consented_ReturnsTrue) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  EXPECT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

INSTANTIATE_TEST_SUITE_P(All, GlicEnablingAnyFreModeTest, testing::Bool());

}  // namespace
}  // namespace glic
