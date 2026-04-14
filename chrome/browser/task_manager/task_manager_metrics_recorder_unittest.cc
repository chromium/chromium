// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_metrics_recorder.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

class TaskManagerMetricsRecorderTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(TaskManagerMetricsRecorderTest, RecordOpenEventOtherIsNoOp) {
  RecordNewOpenEvent(StartAction::kOther);
  histogram_tester_.ExpectTotalCount(kStartActionHistogram, 0);
}

// Parameterized over all non-kOther StartAction values. Automatically covers
// new values when the enum is extended.
class RecordOpenEventTest : public TaskManagerMetricsRecorderTest,
                            public testing::WithParamInterface<int> {};

TEST_P(RecordOpenEventTest, RecordsHistogram) {
  auto action = static_cast<StartAction>(GetParam());
  RecordNewOpenEvent(action);
  histogram_tester_.ExpectBucketCount(kStartActionHistogram, action, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RecordOpenEventTest,
    testing::Range(static_cast<int>(StartAction::kOther) + 1,
                   static_cast<int>(StartAction::kMaxValue) + 1));

TEST_F(TaskManagerMetricsRecorderTest, RecordCloseEvent) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end = start + base::Seconds(5);
  RecordCloseEvent(start, end);
  histogram_tester_.ExpectUniqueTimeSample(kClosedElapsedTimeHistogram,
                                           base::Seconds(5), 1);
}

// Parameterized over all CategoryRecord values. Automatically covers new
// values when the enum is extended.

class RecordTabSwitchEventTest : public TaskManagerMetricsRecorderTest,
                                 public testing::WithParamInterface<int> {};

TEST_P(RecordTabSwitchEventTest, RecordsOneHistogramSample) {
  auto category = static_cast<CategoryRecord>(GetParam());
  RecordTabSwitchEvent(category, base::Seconds(1));
  auto counts =
      histogram_tester_.GetTotalCountsForPrefix("TaskManager.Closed.");
  EXPECT_EQ(1u, counts.size());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RecordTabSwitchEventTest,
    testing::Range(0, static_cast<int>(CategoryRecord::kMaxValue) + 1));

// Spot-checks that CategoryRecord values map to the expected histogram names.
// Catches accidental reordering of kCategoryToString.
static_assert(
    static_cast<int>(CategoryRecord::kMaxValue) == 4,
    "Update TabSwitchHistogramNames test for new CategoryRecord values");
TEST_F(TaskManagerMetricsRecorderTest, TabSwitchHistogramNames) {
  RecordTabSwitchEvent(CategoryRecord::kOther, base::Seconds(1));
  RecordTabSwitchEvent(CategoryRecord::kTabsAndExtensions, base::Seconds(1));
  RecordTabSwitchEvent(CategoryRecord::kBrowser, base::Seconds(1));
  RecordTabSwitchEvent(CategoryRecord::kSystem, base::Seconds(1));
  RecordTabSwitchEvent(CategoryRecord::kAll, base::Seconds(1));
  histogram_tester_.ExpectTotalCount("TaskManager.Closed.Other.ElapsedTime", 1);
  histogram_tester_.ExpectTotalCount(
      "TaskManager.Closed.TabsAndExtensions.ElapsedTime", 1);
  histogram_tester_.ExpectTotalCount("TaskManager.Closed.Browser.ElapsedTime",
                                     1);
  histogram_tester_.ExpectTotalCount("TaskManager.Closed.System.ElapsedTime",
                                     1);
  histogram_tester_.ExpectTotalCount("TaskManager.Closed.All.ElapsedTime", 1);
}

// Verifies end_process_count maps to the correct ordinal histogram name via
// kEndProcessCountToString in task_manager_metrics_recorder.cc.

struct EndProcessCase {
  size_t count;
  const char* expected_ordinal;
};

class RecordEndProcessEventTest
    : public TaskManagerMetricsRecorderTest,
      public testing::WithParamInterface<EndProcessCase> {};

TEST_P(RecordEndProcessEventTest, MapsCountToOrdinalHistogram) {
  const auto& [count, expected_ordinal] = GetParam();
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end = start + base::Seconds(1);
  RecordEndProcessEvent(start, end, count);
  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(kTimeToEndProcessHistogram, expected_ordinal), 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         RecordEndProcessEventTest,
                         testing::Values(EndProcessCase{1, "First"},
                                         EndProcessCase{2, "Second"},
                                         EndProcessCase{3, "Third"},
                                         EndProcessCase{4, "Fourth"},
                                         EndProcessCase{5, "Fifth"}));

TEST_F(TaskManagerMetricsRecorderTest, RecordEndProcessZeroIsNoOp) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end = start + base::Seconds(1);
  RecordEndProcessEvent(start, end, 0);
  EXPECT_EQ(0, histogram_tester_.GetTotalCountForPrefix("TaskManager."));
}

TEST_F(TaskManagerMetricsRecorderTest, RecordEndProcessSixthIsNoOp) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end = start + base::Seconds(1);
  RecordEndProcessEvent(start, end, 6);
  EXPECT_EQ(0, histogram_tester_.GetTotalCountForPrefix("TaskManager."));
}

}  // namespace task_manager
