// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"

using ChromeAcceptHeaderTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeAcceptHeaderTest, Check) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTP);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  std::string plugin_accept_header, favicon_accept_header;
  base::RunLoop plugin_loop, favicon_loop;
  server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/pdf/test.pdf") {
          auto it = request.headers.find("Accept");
          if (it != request.headers.end())
            plugin_accept_header = it->second;
          plugin_loop.Quit();
        } else if (request.relative_url == "/favicon.ico") {
          auto it = request.headers.find("Accept");
          if (it != request.headers.end())
            favicon_accept_header = it->second;
          favicon_loop.Quit();
        }
      }));
  ASSERT_TRUE(server.Start());
  GURL url = server.GetURL("/pdf/pdf_embed.html");
  ui_test_utils::NavigateToURL(browser(), url);

  plugin_loop.Run();
  favicon_loop.Run();

  if (content::MimeHandlerViewMode::UsesCrossProcessFrame()) {
    // With MimeHandlerViewInCrossProcessFrame, embedded PDF will go through the
    // navigation code path and behaves similarly to PDF loaded inside <iframe>.
    ASSERT_EQ(
        "text/html,application/xhtml+xml,application/xml;q=0.9,image/"
        "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9",
        plugin_accept_header);
  } else {
    // This is for a sub-resource request.
    ASSERT_EQ("*/*", plugin_accept_header);
  }

  ASSERT_EQ("image/webp,image/apng,image/*,*/*;q=0.8", favicon_accept_header);

  // Since the server uses local variables.
  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}
