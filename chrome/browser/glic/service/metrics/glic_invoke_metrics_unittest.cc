// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicInvokeMetricsTest, ConstructorRecordsSource) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);

  histogram_tester.ExpectUniqueSample("Glic.Invoke.InvocationSource",
                                      mojom::InvocationSource::kOsButton, 1);
}

TEST(GlicInvokeMetricsTest, RecordSuccess) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordSuccess();

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeResult::kSuccess, 1);
}

TEST(GlicInvokeMetricsTest, RecordError) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);
  metrics.RecordError(GlicInvokeError::kInvalidTab);

  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kInvalidTab, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kInvalidTab, 1);
}

TEST(GlicInvokeMetricsTest, RecordDurationIsCaptured) {
  base::HistogramTester histogram_tester;
  GlicInvokeMetrics metrics(mojom::InvocationSource::kOsButton);

  // Note: Since time advances inside the method, we test that SOME sample is
  // recorded.
  metrics.RecordSuccess();

  histogram_tester.ExpectTotalCount("Glic.Invoke.Duration", 1);
  histogram_tester.ExpectTotalCount("Glic.Invoke.Duration.OsButton", 1);
}

}  // namespace
}  // namespace glic
