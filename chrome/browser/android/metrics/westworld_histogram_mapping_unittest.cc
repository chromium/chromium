// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/westworld_histogram_mapping.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome::android::westworld {

TEST(WestworldHistogramMappingTest, GetAtomMappingInfo_TabCount) {
  auto info = GetAtomMappingInfo("Tabs.TabCount");

  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->atom_id, 215200);
  EXPECT_EQ(info->type, MetricType::kInt);
}

TEST(WestworldHistogramMappingTest, GetAtomMappingInfo_WindowCount) {
  auto info = GetAtomMappingInfo("Tabs.WindowCount");

  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->atom_id, 215201);
  EXPECT_EQ(info->type, MetricType::kInt);
}

TEST(WestworldHistogramMappingTest, GetAtomMappingInfo_UnmappedHistogram) {
  auto info = GetAtomMappingInfo("Some.Unmapped.Histogram");

  EXPECT_FALSE(info.has_value());
}

}  // namespace chrome::android::westworld
