// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_session_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(PickerSessionMetricsTest, RecordsFirstFocusLatency) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram;

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment.FastForwardBy(base::Seconds(1));
  PickerSessionMetrics metrics(trigger_event_timestamp);
  task_environment.FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST(PickerSessionMetricsTest, RecordsOnlyFirstFocusLatency) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram;

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment.FastForwardBy(base::Seconds(1));
  PickerSessionMetrics metrics(trigger_event_timestamp);
  task_environment.FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();
  // Mark a second focus. Only the first focus should be recorded.
  task_environment.FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

}  // namespace
}  // namespace ash
