// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using PickerSessionMetricsTest = testing::Test;

TEST_F(PickerSessionMetricsTest, RecordsSessionOutcomeOnce) {
  base::HistogramTester histogram;
  PickerSessionMetrics metrics;

  metrics.RecordOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  metrics.RecordOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kAbandoned);
  metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kUnknown);

  histogram.ExpectUniqueSample(
      "Ash.Picker.Session.Outcome",
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied, 1);
}

TEST_F(PickerSessionMetricsTest, RecordsUnknownOutcomeOnDestruction) {
  base::HistogramTester histogram;
  { PickerSessionMetrics metrics; }

  histogram.ExpectUniqueSample("Ash.Picker.Session.Outcome",
                               PickerSessionMetrics::SessionOutcome::kUnknown,
                               1);
}

TEST_F(PickerSessionMetricsTest,
       DoesNotRecordUnknownOutcomeOnDestructionIfOutcomeWasRecorded) {
  base::HistogramTester histogram;
  {
    PickerSessionMetrics metrics;
    metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kAbandoned);
  }

  histogram.ExpectUniqueSample("Ash.Picker.Session.Outcome",
                               PickerSessionMetrics::SessionOutcome::kAbandoned,
                               1);
}

}  // namespace
}  // namespace ash
