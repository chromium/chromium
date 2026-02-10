// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_pdh_metrics_provider_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/win/scoped_pdh_query.h"
#include "testing/gmock/include/gmock/gmock.h"

class PdhMetricsProviderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

// Verifies that we're emitting histograms from the PdhQueryHandler after
// OnRecordingEnabled() is called.
TEST_F(PdhMetricsProviderTest, RecordsHistograms) {
  SystemPdhMetricsProvider provider;
  provider.OnRecordingEnabled();

  // Windows requires at least one second to have passed between recordings of
  // the performance counters.
  base::PlatformThread::Sleep(base::Seconds(1));
  environment_.FastForwardBy(base::Seconds(6));

  histogram_tester_.ExpectTotalCount(provider.kHardFaultCountHistogram, 1);
  histogram_tester_.ExpectTotalCount(provider.kDemandZeroFaultCountHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(provider.kUserKernelRatioHistogram, 1);
  histogram_tester_.ExpectTotalCount(provider.kUserTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(provider.kKernelTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(provider.kPagefileUtilizationHistogram, 1);

  histogram_tester_.ExpectTotalCount(
      base::win::ScopedPdhQuery::kQueryErrorHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      base::win::ScopedPdhQuery::kResultErrorHistogram, 0);
}
