// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

enum WebSocketCreationPolicy {
  IN_MAIN_FRAME,
  IN_WORKER,
};
class SubresourceFilterWebSocketBrowserTest
    : public SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<WebSocketCreationPolicy> {
 public:
  SubresourceFilterWebSocketBrowserTest() {}

  SubresourceFilterWebSocketBrowserTest(
      const SubresourceFilterWebSocketBrowserTest&) = delete;
  SubresourceFilterWebSocketBrowserTest& operator=(
      const SubresourceFilterWebSocketBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    websocket_test_server_ = std::make_unique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server_->Start());
  }

  net::SpawnedTestServer* websocket_test_server() {
    return websocket_test_server_.get();
  }

  GURL GetWebSocketUrl(const std::string& path) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("ws");
    return websocket_test_server_->GetURL(path).ReplaceComponents(replacements);
  }

  void CreateWebSocketAndExpectResult(const GURL& url,
                                      bool expect_connection_success) {
    EXPECT_EQ(
        expect_connection_success,
        content::EvalJs(
            browser()->tab_strip_model()->GetActiveWebContents(),
            base::StringPrintf("connectWebSocket('%s');", url.spec().c_str())));
  }

 private:
  std::unique_ptr<net::SpawnedTestServer> websocket_test_server_;
};

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest, BlockWebSocket) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("echo-with-no-extension"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CreateWebSocketAndExpectResult(websocket_url,
                                 false /* expect_connection_success */);
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest,
                       DoNotBlockWebSocketNoActivatedFrame) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("echo-with-no-extension"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  CreateWebSocketAndExpectResult(websocket_url,
                                 true /* expect_connection_success */);
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest,
                       DoNotBlockWebSocketInActivatedFrameWithNoRule) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ConfigureAsPhishingURL(url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  CreateWebSocketAndExpectResult(websocket_url,
                                 true /* expect_connection_success */);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubresourceFilterWebSocketBrowserTest,
    ::testing::Values(WebSocketCreationPolicy::IN_WORKER,
                      WebSocketCreationPolicy::IN_MAIN_FRAME));

}  // namespace subresource_filter
