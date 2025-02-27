// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::FeatureRef;

namespace glic {
namespace {

class GlicEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable kGlic and kTabstripComboButton by default for testing.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");

    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    scoped_feature_list_.Reset();
  }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
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

TEST_F(GlicEnablingTest, EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  ForceSigninAndModelExecutionCapability(profile());
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

}  // namespace
}  // namespace glic
