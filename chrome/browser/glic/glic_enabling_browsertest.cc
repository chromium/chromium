// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
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
  GlicEnablingTest() {
    // Enable kGlic and kTabstripComboButton by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
  }
  ~GlicEnablingTest() override = default;

  void TearDown() override {
    scoped_feature_list_.Reset();
    InProcessBrowserTest::TearDown();
  }

 protected:
  Profile* profile() { return browser()->profile(); }
  ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }
  ProfileAttributesStorage& attributes_storage() {
    return profile_manager()->GetProfileAttributesStorage();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test
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

}  // namespace
}  // namespace glic
