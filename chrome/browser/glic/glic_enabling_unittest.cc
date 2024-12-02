// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_enabling.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
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
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
  }

 protected:
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

}  // namespace
}  // namespace glic
