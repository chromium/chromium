// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_pdh_metrics_provider_win.h"

#include <windows.h>

#include <ntstatus.h>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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

// Verifies that we're emitting per-process histograms from the PdhQueryHandler.
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
  base::PlatformThread::Sleep(base::Seconds(1));
  environment_.FastForwardBy(base::Seconds(30));

  if (histogram_tester_.GetBucketCount(
          base::win::ScopedPdhQuery::kQueryErrorHistogram, -1073738824) == 0) {
    for (const char* suffix : {"", ".FirstSample"}) {
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.UserTime.Browser", suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.PrivilegedTime.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.HandleCount.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.IODataBytesPerSec.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Windows.Experimental.Pdh.ProcessV2."
                        "IODataOperationsPerSec.Browser",
                        suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.IOOtherBytesPerSec.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.IOReadBytesPerSec.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Windows.Experimental.Pdh.ProcessV2."
                        "IOReadOperationsPerSec.Browser",
                        suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.IOWriteBytesPerSec.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Windows.Experimental.Pdh.ProcessV2."
                        "IOWriteOperationsPerSec.Browser",
                        suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.PageFaultsPerSec.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.PageFileBytes.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.PageFileBytesPeak.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.PrivateBytes.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.ThreadCount.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Windows.Experimental.Pdh.ProcessV2.WorkingSet.Browser",
                        suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.WorkingSetPrivate.Browser",
               suffix}),
          1);
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Windows.Experimental.Pdh.ProcessV2.WorkingSetPeak.Browser",
               suffix}),
          1);
    }

    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 0);
  } else {
    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 18);
  }
}

TEST_F(PdhMetricsProviderTest, RecordsSubsetOfChildProcessHistograms) {
  if (base::win::GetVersion() < base::win::Version::WIN11) {
    GTEST_SKIP() << "Not supported prior to Win11";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kSystemPdhMetrics,
      {{"system_pdh_metrics_metrics_per_process", "3"}});

  SystemPdhMetricsProvider provider;
  provider.OnRecordingEnabled();

  // Windows requires at least one second to have passed between recordings of
  // the performance counters.
  environment_.FastForwardBy(base::Seconds(35));
  base::PlatformThread::Sleep(base::Seconds(1));
  environment_.FastForwardBy(base::Seconds(30));
  base::PlatformThread::Sleep(base::Seconds(1));
  environment_.FastForwardBy(base::Seconds(30));

  // -1073738824 is PDH_CSTATUS_NO_OBJECT, which occurs if the performance
  // counter object is unavailable in this environment (e.g. on test bots/VMs).
  // If no such error occurred, verify that metrics were recorded.
  if (histogram_tester_.GetBucketCount(
          base::win::ScopedPdhQuery::kQueryErrorHistogram, -1073738824) == 0) {
    static constexpr std::string_view kMetrics[] = {
        "UserTime",
        "PrivilegedTime",
        "HandleCount",
        "IODataBytesPerSec",
        "IODataOperationsPerSec",
        "IOOtherBytesPerSec",
        "IOReadBytesPerSec",
        "IOReadOperationsPerSec",
        "IOWriteBytesPerSec",
        "IOWriteOperationsPerSec",
        "PageFaultsPerSec",
        "PageFileBytes",
        "PageFileBytesPeak",
        "PrivateBytes",
        "ThreadCount",
        "WorkingSet",
        "WorkingSetPrivate",
        "WorkingSetPeak",
    };

    int recorded_metrics_count = 0;
    for (std::string_view metric : kMetrics) {
      std::string name = base::StrCat(
          {"Windows.Experimental.Pdh.ProcessV2.", metric, ".Browser"});
      size_t count = histogram_tester_.GetTotalCountsForPrefix(name).size();
      if (count > 0) {
        ++recorded_metrics_count;
        histogram_tester_.ExpectTotalCount(name, 1);
        histogram_tester_.ExpectTotalCount(base::StrCat({name, ".FirstSample"}),
                                           1);
      }
    }

    EXPECT_EQ(recorded_metrics_count, 3);

    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 0);
  } else {
    histogram_tester_.ExpectTotalCount(
        base::win::ScopedPdhQuery::kQueryErrorHistogram, 3);
  }
}
