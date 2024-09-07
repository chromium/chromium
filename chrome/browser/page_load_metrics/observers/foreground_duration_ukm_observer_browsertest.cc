// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::PageForegroundSession;

class ForegroundDurationUKMObserverBrowserTest : public InProcessBrowserTest {
 public:
  ForegroundDurationUKMObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ForegroundDurationUKMObserverBrowserTest::web_contents,
            base::Unretained(this))) {}

  ForegroundDurationUKMObserverBrowserTest(
      const ForegroundDurationUKMObserverBrowserTest&) = delete;
  ForegroundDurationUKMObserverBrowserTest& operator=(
      const ForegroundDurationUKMObserverBrowserTest&) = delete;

  ~ForegroundDurationUKMObserverBrowserTest() override {}

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  void StartHttpsServer(net::EmbeddedTestServer::ServerCertificate cert) {
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(cert);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }
  void ExpectMetricCountForUrl(const GURL& url,
                               const char* metric_name,
                               const int expected_count) {
    int count = 0;
    for (const ukm::mojom::UkmEntry* entry :
         test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url &&
          test_ukm_recorder_->EntryHasMetric(entry, metric_name)) {
        count++;
      }
    }
    EXPECT_EQ(count, expected_count);
  }

  void CloseAllTabs() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest, RecordSimple) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CloseAllTabs();
  ExpectMetricCountForUrl(url, "ForegroundDuration", 1);
}

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest,
                       PrerenderActivationInForeground) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL empty = https_test_server()->GetURL("/empty.html");
  GURL simple = https_test_server()->GetURL("/simple.html");
  prerender_helper().NavigatePrimaryPage(empty);
  content::FrameTreeNodeId host_id = prerender_helper().AddPrerender(simple);
  prerender_helper().WaitForPrerenderLoadCompletion(host_id);

  ExpectMetricCountForUrl(simple, "ForegroundDuration", 0);
  prerender_helper().NavigatePrimaryPage(simple);
  CloseAllTabs();

  // The page was activated in foreground. The metrics should be recorded.
  ExpectMetricCountForUrl(simple, "ForegroundDuration", 1);
}

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest,
                       PrerenderActivationInBackground) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL empty = https_test_server()->GetURL("/empty.html");
  GURL simple = https_test_server()->GetURL("/simple.html");
  prerender_helper().NavigatePrimaryPage(empty);
  content::FrameTreeNodeId host_id = prerender_helper().AddPrerender(simple);
  prerender_helper().WaitForPrerenderLoadCompletion(host_id);

  // Make the initiator page occluded. This will be treated as a background
  // page. Note that we cannot make the initiator page hidden here as a hidden
  // page cannot activate a prerendered page.
  web_contents()->WasOccluded();

  ExpectMetricCountForUrl(simple, "ForegroundDuration", 0);
  prerender_helper().NavigatePrimaryPage(simple);
  CloseAllTabs();

  // The page was activated in background. The metrics should not be recorded.
  ExpectMetricCountForUrl(simple, "ForegroundDuration", 0);
}

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest, TabSwitching) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url1 = https_test_server()->GetURL("/simple.html");
  GURL url2 = https_test_server()->GetURL("/form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(url1, tab_strip_model->GetWebContentsAt(0)->GetLastCommittedURL());
  EXPECT_EQ(url2, tab_strip_model->GetWebContentsAt(1)->GetLastCommittedURL());

  tab_strip_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip_model->CloseAllTabs();
  ExpectMetricCountForUrl(url1, "ForegroundDuration", 3);
  ExpectMetricCountForUrl(url2, "ForegroundDuration", 3);
}
