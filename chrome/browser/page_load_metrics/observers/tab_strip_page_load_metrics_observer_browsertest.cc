// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/tab_strip_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"

class TabStripPageLoadMetricsObserverTest : public InProcessBrowserTest {
 protected:
  void StartHttpsServer(net::EmbeddedTestServer::ServerCertificate cert) {
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(cert);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

IN_PROC_BROWSER_TEST_F(TabStripPageLoadMetricsObserverTest,
                       RecordPageLoadMetrics) {
  base::HistogramTester histogram_tester;
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  histogram_tester.ExpectTotalCount(internal::kTabsPageLoadTimeSinceActive, 2);
  histogram_tester.ExpectTotalCount(internal::kTabsPageLoadTimeSinceCreated, 2);
}

IN_PROC_BROWSER_TEST_F(TabStripPageLoadMetricsObserverTest,
                       RecordActiveMetrics) {
  base::HistogramTester histogram_tester;
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->ActivateTabAt(
      2, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  histogram_tester.ExpectUniqueSample(internal::kTabsActiveAbsolutePosition, 3,
                                      1);
  histogram_tester.ExpectUniqueSample(internal::kTabsActiveRelativePosition, 60,
                                      1);
}
