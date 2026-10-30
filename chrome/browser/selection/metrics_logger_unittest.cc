// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace selection {
namespace {

TEST(MetricsLoggerTest, PreselectedRegionLogsImmediately) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kContextMenuText, "application/pdf",
                         /*has_preselected_region=*/true);
    logger.RecordSessionEnd(/*cancelled_by_user=*/true);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      EntryPoint::kContextMenuText, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Invocation.ByDocumentMimeType", DocumentMimeType::kPdf,
      1);
  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kPreselectedText, 1);
}

TEST(MetricsLoggerTest, SpatialSelectionCompleted) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
    logger.RecordSpatialSelectionCompleted();
    logger.RecordSpatialSelectionCompleted();
  }

  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Selection.Outcome",
      SelectionOutcome::kSpatialSelectionCompleted, 1);
}

TEST(MetricsLoggerTest, CancelledAtSelectionStage) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
    logger.RecordSessionEnd(/*cancelled_by_user=*/true);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kCancelledByUser, 1);
}

TEST(MetricsLoggerTest, AbandonedAtSelectionStage) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kAbandonedOrError, 1);
}

}  // namespace
}  // namespace selection
