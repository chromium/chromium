// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_pdh_metrics_provider_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/win/scoped_pdh_query.h"
#include "base/win/windows_version.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

class PdhMetricsProviderTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment environment_{
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
  environment_.FastForwardBy(base::Seconds(35));

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

// Verifies that we're emitting histograms from the PdhQueryHandler after
// a process is created.
TEST_F(PdhMetricsProviderTest, RecordsChildProcessHistograms) {
  if (base::win::GetVersion() < base::win::Version::WIN11) {
    GTEST_SKIP() << "Not supported prior to Win11";
  }
  SystemPdhMetricsProvider provider;

  provider.OnRecordingEnabled();

  // Windows requires at least one second to have passed between recordings of
  // the performance counters.
  environment_.FastForwardBy(base::Seconds(35));
  base::PlatformThread::Sleep(base::Seconds(1));
  environment_.FastForwardBy(base::Seconds(30));

  if (histogram_tester_.GetBucketCount(
          base::win::ScopedPdhQuery::kQueryErrorHistogram, -1073738824) == 0) {
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.UserTime.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.PrivilegedTime.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.HandleCount.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IODataBytesPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IODataOperationsPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IOOtherBytesPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IOReadBytesPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IOReadOperationsPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IOWriteBytesPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.IOWriteOperationsPerSec.Browser",
        1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.PageFaultsPerSec.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.PageFileBytes.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.PageFileBytesPeak.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.PrivateBytes.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.ThreadCount.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.WorkingSet.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.WorkingSetPrivate.Browser", 1);
    histogram_tester_.ExpectTotalCount(
        "Windows.Experimental.Pdh.ProcessV2.WorkingSetPeak.Browser", 1);

    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 0);
    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kResultErrorHistogram, 0);
  } else {
    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 18);
  }
}
