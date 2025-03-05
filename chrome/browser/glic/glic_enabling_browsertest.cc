// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test
IN_PROC_BROWSER_TEST_F(GlicEnablingTest, EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  Profile* profile = browser()->profile();
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile));
  ForceSigninAndModelExecutionCapability(profile);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
}

}  // namespace
}  // namespace glic
