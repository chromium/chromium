// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"

#include <map>
#include <string>

#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features_test_helper.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_ranker {

using assist_ranker::RankerExample;
using ukm::builders::TabManager_TabMetrics;
using ukm::TestUkmRecorder;

// This ensures that all field in TabFeatures populates to RankerExample
// correctly.
TEST(TabFeaturesTest, PopulateTabFeaturesToRankerExample) {
  const TabFeatures tab = GetFullTabFeaturesForTesting();

  RankerExample example;
  PopulateTabFeaturesToRankerExample(tab, &example);

  const auto& features = example.features();

  // This ensures all tab features are populated into example.
  EXPECT_EQ(features.size(), 26U);

  EXPECT_EQ(features.at(TabManager_TabMetrics::kHasBeforeUnloadHandlerName)
                .bool_value(),
            1);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kHasFormEntryName).bool_value(),
            1);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kIsPinnedName).bool_value(), 1);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kKeyEventCountName).int32_value(), 21);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kMouseEventCountName).int32_value(),
      22);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kMRUIndexName).int32_value(),
            27);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kNavigationEntryCountName)
                .int32_value(),
            24);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kNumReactivationBeforeName)
                .int32_value(),
            25);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kPageTransitionCoreTypeName)
                .int32_value(),
            2);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kPageTransitionFromAddressBarName)
          .bool_value(),
      1);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kPageTransitionIsRedirectName)
                .bool_value(),
            1);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kSiteEngagementScoreName)
                .int32_value(),
            26);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kTimeFromBackgroundedName)
                .int32_value(),
            10000);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kTotalTabCountName).int32_value(), 30);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kTouchEventCountName).int32_value(),
      28);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kWasRecentlyAudibleName).bool_value(),
      1);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kWindowIsActiveName).bool_value(), 1);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kWindowShowStateName).int32_value(),
      3);
  EXPECT_EQ(
      features.at(TabManager_TabMetrics::kWindowTabCountName).int32_value(),
      27);
  EXPECT_EQ(features.at(TabManager_TabMetrics::kWindowTypeName).int32_value(),
            4);

  EXPECT_EQ(features.at("IsActive").bool_value(), 1);
  EXPECT_EQ(features.at("ShowState").int32_value(), 3);
  EXPECT_EQ(features.at("TabCount").int32_value(), 27);
  EXPECT_EQ(features.at("Type").int32_value(), 4);
  EXPECT_FLOAT_EQ(features.at("NormalizedMRUIndex").float_value(), 0.9f);
  EXPECT_EQ(features.at("TopDomain").string_value(), "726059442646517786");
}

// This ensures that all field in TabFeatures populates to TabManager_TabMetrics
// correctly.
TEST(TabFeaturesTest, PopulateTabFeaturesToUkmEntry) {
  // Sets up the task scheduling/task-runner environment for each test.
  base::test::TaskEnvironment task_environment;
  // Sets itself as the global UkmRecorder on construction.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const TabFeatures tab = GetFullTabFeaturesForTesting();

  ukm::builders::TabManager_TabMetrics tab_entry(0);
  PopulateTabFeaturesToUkmEntry(tab, &tab_entry);
  tab_entry.Record(ukm::UkmRecorder::Get());

  const ukm::mojom::UkmEntry* entry =
      test_ukm_recorder.GetEntriesByName(TabManager_TabMetrics::kEntryName)[0];

  std::map<std::string, int64_t> expected_metrics = {
      {"HasBeforeUnloadHandler", 1},
      {"HasFormEntry", 1},
      {"IsPinned", 1},
      {"KeyEventCount", 21},
      {"MouseEventCount", 22},
      {"MRUIndex", 27},
      {"NavigationEntryCount", 24},
      {"NumReactivationBefore", 25},
      {"PageTransitionCoreType", 2},
      {"PageTransitionFromAddressBar", 1},
      {"PageTransitionIsRedirect", 1},
      {"SiteEngagementScore", 26},
      {"TimeFromBackgrounded", 10000},
      {"TotalTabCount", 30},
      {"TouchEventCount", 28},
      {"WasRecentlyAudible", 1},
      {"WindowIsActive", 1},
      {"WindowShowState", 3},
      {"WindowTabCount", 27},
      {"WindowType", 4},
  };
  // Check all metrics are logged as expected.
  EXPECT_EQ(entry->metrics.size(), expected_metrics.size());

  for (const auto& pair : expected_metrics) {
    test_ukm_recorder.ExpectEntryMetric(entry, pair.first, pair.second);
  }
}

}  // namespace tab_ranker
