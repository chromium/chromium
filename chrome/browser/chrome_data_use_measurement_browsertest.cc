// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

class ChromeDataUseMeasurementBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  size_t GetTotalDataUse() const {
    return histogram_tester_.GetTotalSum("DataUse.BytesReceived3.Delegate") +
           histogram_tester_.GetTotalSum("DataUse.BytesSent3.Delegate");
  }

  void RetryUntilUserInitiatedDataUsePrefHasEntry() {
    do {
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop().RunUntilIdle();
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    } while (GetTotalDataUse() == 0);
  }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ChromeDataUseMeasurementBrowserTest, DataUseRecorded) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  RetryUntilUserInitiatedDataUsePrefHasEntry();

  EXPECT_GT(GetTotalDataUse(), 0u);
}
