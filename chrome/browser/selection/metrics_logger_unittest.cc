// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace selection {
namespace {

TEST(MetricsLoggerTest, LogsInvocationEntryPointAndMimeType) {
  base::HistogramTester histogram_tester;
  const MetricsLogger logger(EntryPoint::kContextMenuText, "application/pdf");

  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      EntryPoint::kContextMenuText, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Invocation.ByDocumentMimeType", DocumentMimeType::kPdf,
      1);
}

}  // namespace
}  // namespace selection
