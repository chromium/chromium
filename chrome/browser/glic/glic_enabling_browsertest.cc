// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::FeatureRef;

namespace glic {
namespace {

class GlicEnablingTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    InProcessBrowserTest::TearDown();
  }

 protected:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         features::kGlicRollout},
        {});
  }

  Profile* profile() { return browser()->profile(); }
  ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }
  ProfileAttributesStorage& attributes_storage() {
    return profile_manager()->GetProfileAttributesStorage();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  ForceSigninAndModelExecutionCapability(profile());
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, AttributeEntryUpdatesOnChange) {
  SigninWithPrimaryAccount(profile());
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  ProfileAttributesEntry* entry =
      attributes_storage().GetAllProfilesAttributes().front();
  EXPECT_FALSE(entry->IsGlicEligible());

  // Setting the model execution capability updates the glic AttributeEntry.
  SetModelExecutionCapability(profile(), true);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  ASSERT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  EXPECT_TRUE(entry->IsGlicEligible());
}

class GlicEnablingTieredRolloutTest : public GlicEnablingTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         features::kGlicTieredRollout},
        {features::kGlicRollout});
  }
  ~GlicEnablingTieredRolloutTest() override = default;

  void SetTieredRolloutEligibilityForProfile(bool is_eligible) {
    profile()->GetPrefs()->SetInteger(
        prefs::kGlicRolloutEligibility,
        is_eligible
            ? static_cast<int>(
                  prefs::RolloutEligibility::kEligibleTieredRollout)
            : static_cast<int>(prefs::RolloutEligibility::kNotEligible));
  }
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTieredRolloutTest, EnabledForProfileTest) {
  ForceSigninAndModelExecutionCapability(profile());

  // Should not be enabled as profile not eligible for tiered rollout.
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  // Should be enabled as now eligible for tiered rollout.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Simulate user no longer eligible.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/false);
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingTieredRolloutTest,
                       InTieredRolloutGroupOtherCriteriaNotPassing) {
  // Should be enabled as profile.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
}

class GlicEnablingSimultaneousRolloutTest
    : public GlicEnablingTieredRolloutTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         features::kGlicTieredRollout, features::kGlicRollout},
        {});
  }
  ~GlicEnablingSimultaneousRolloutTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingSimultaneousRolloutTest,
                       EnabledForProfileTest) {
  ForceSigninAndModelExecutionCapability(profile());

  // Eligible for tiered rollout. Profile enabled for GLIC.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // No longer eligible for tiered rollout. Should not have effect on overall
  // enablement.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/false);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

}  // namespace
}  // namespace glic
