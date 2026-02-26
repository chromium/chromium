// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_sync_prefs_utils.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AccessibilitySyncPrefsTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AccessibilitySyncPrefsTest, NoBatchesEnabledReturnsEmpty) {
  {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kOsSyncAccessibilitySettingsBatch1,
            features::kOsSyncAccessibilitySettingsBatch2,
            features::kOsSyncAccessibilitySettingsBatch3,
        });

    auto prefs = GetAccessibilityPrefBatchesWithSyncEnabled();
    EXPECT_TRUE(prefs.empty());
  }

  {
    scoped_feature_list_.Reset();
    auto prefs = GetAccessibilityPrefBatchesWithSyncEnabled();
    EXPECT_TRUE(prefs.empty());
  }
}

TEST_F(AccessibilitySyncPrefsTest, OnlyEnabledBatchesReturned) {
  {  // Batch 1.
    scoped_feature_list_.InitWithFeatures(
        {features::kOsSyncAccessibilitySettingsBatch1}, {});
    auto prefs = GetAccessibilityPrefBatchesWithSyncEnabled();
    EXPECT_FALSE(prefs.empty());
  }

  {  // Batch 2.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kOsSyncAccessibilitySettingsBatch2}, {});
    auto prefs = GetAccessibilityPrefBatchesWithSyncEnabled();
    EXPECT_FALSE(prefs.empty());
  }

  {  // Batch 3.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kOsSyncAccessibilitySettingsBatch3}, {});
    auto prefs = GetAccessibilityPrefBatchesWithSyncEnabled();
    EXPECT_FALSE(prefs.empty());
  }
}

}  // namespace ash
