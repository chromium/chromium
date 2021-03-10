// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using PageFreezerBrowserTest = InProcessBrowserTest;

void MaybeFreezePageNode(content::WebContents* contents, bool expect_success) {
  base::RunLoop run_loop;

  auto reply_cb = base::BindOnce(
      [](bool expect_success, base::OnceClosure quit_closure, bool success) {
        EXPECT_EQ(expect_success, success);
        std::move(quit_closure).Run();
      },
      expect_success, run_loop.QuitClosure());

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<PageNode> page_node,
                        base::OnceCallback<void(bool)> reply_cb) {
                       EXPECT_TRUE(page_node);
                       mechanism::PageFreezer freezer;
                       freezer.MaybeFreezePageNodeWithReplyForTesting(
                           page_node.get(), std::move(reply_cb));
                     },
                     PerformanceManager::GetPageNodeForWebContents(contents),
                     std::move(reply_cb)));
  run_loop.Run();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PageFreezerBrowserTest, FreezePageNode) {
  // Simple test to freeze a WebContents that is in a freezable state.
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  MaybeFreezePageNode(contents, true);
}

IN_PROC_BROWSER_TEST_F(PageFreezerBrowserTest,
                       FreezePageNodeWithNoStoreCacheControlHeader) {
  // Ensure that a WebContents that sets the "Cache-Control: no-store" header
  // can't be frozen.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
            new net::test_server::BasicHttpResponse);
        http_response->set_code(net::HTTP_OK);
        http_response->AddCustomHeader("Cache-Control", "no-store");
        http_response->set_content("foo");
        return http_response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto* contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  MaybeFreezePageNode(contents, false);
}

}  // namespace performance_manager
