// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "content/public/test/browser_test_utils.h"
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
  ForegroundDurationUKMObserverBrowserTest() {}
  ~ForegroundDurationUKMObserverBrowserTest() override {}

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
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
    for (auto* entry :
         test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url) {
        test_ukm_recorder_->EntryHasMetric(entry, metric_name);
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
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;

  DISALLOW_COPY_AND_ASSIGN(ForegroundDurationUKMObserverBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest, RecordSimple) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");
  ui_test_utils::NavigateToURL(browser(), url);
  CloseAllTabs();
  ExpectMetricCountForUrl(url, "ForegroundDuration", 1);
}

IN_PROC_BROWSER_TEST_F(ForegroundDurationUKMObserverBrowserTest, TabSwitching) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url1 = https_test_server()->GetURL("/simple.html");
  GURL url2 = https_test_server()->GetURL("/form.html");
  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(url1, tab_strip_model->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(url2, tab_strip_model->GetWebContentsAt(1)->GetURL());

  tab_strip_model->ActivateTabAt(0, {TabStripModel::GestureType::kOther});
  tab_strip_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  tab_strip_model->ActivateTabAt(0, {TabStripModel::GestureType::kOther});
  tab_strip_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  tab_strip_model->CloseAllTabs();
  ExpectMetricCountForUrl(url1, "ForegroundDuration", 3);
  ExpectMetricCountForUrl(url2, "ForegroundDuration", 3);
}
