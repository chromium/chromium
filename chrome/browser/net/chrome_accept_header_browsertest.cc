// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

using ChromeAcceptHeaderTest = InProcessBrowserTest;

#if BUILDFLAG(ENABLE_AV1_DECODER) || BUILDFLAG(ENABLE_JXL_DECODER)
namespace {
std::string GetOptionalImageCodecs() {
  std::string result;
#if BUILDFLAG(ENABLE_JXL_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kJXL)) {
    result.append("image/jxl,");
  }
#endif
#if BUILDFLAG(ENABLE_AV1_DECODER)
  result.append("image/avif,");
#endif
  return result;
}
}  // namespace
#endif  // BUILDFLAG(ENABLE_AV1_DECODER) || BUILDFLAG(ENABLE_JXL_DECODER)

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

  // With MimeHandlerViewInCrossProcessFrame, embedded PDF will go through the
  // navigation code path and behaves similarly to PDF loaded inside <iframe>.
  std::string expected_plugin_accept_header =
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/"
      "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9";
#if BUILDFLAG(ENABLE_AV1_DECODER) || BUILDFLAG(ENABLE_JXL_DECODER)
  expected_plugin_accept_header =
      "text/html,application/xhtml+xml,application/xml;q=0.9," +
      GetOptionalImageCodecs() +
      "image/webp,image/apng,*/*;q=0.8,"
      "application/signed-exchange;v=b3;q=0.9";
#endif
  ASSERT_EQ(expected_plugin_accept_header, plugin_accept_header);

  std::string expected_favicon_accept_header =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#if BUILDFLAG(ENABLE_AV1_DECODER) || BUILDFLAG(ENABLE_JXL_DECODER)
  expected_favicon_accept_header =
      GetOptionalImageCodecs() +
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif
  ASSERT_EQ(expected_favicon_accept_header, favicon_accept_header);

  // Since the server uses local variables.
  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}
