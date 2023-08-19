// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/media_buildflags.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/http_request.h"

class ChromeAcceptEncodingHeaderTest : public InProcessBrowserTest {
 public:
  ChromeAcceptEncodingHeaderTest() {
    feature_list_.InitAndEnableFeature(net::features::kZstdContentEncoding);
  }
  ~ChromeAcceptEncodingHeaderTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAcceptEncodingHeaderTest, Check) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  std::string navigation_accept_encoding_header,
      subresource_accept_encoding_header;
  base::RunLoop navigation_loop, subresource_loop;
  server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/fetch.html") {
          auto it = request.headers.find("Accept-Encoding");
          if (it != request.headers.end())
            navigation_accept_encoding_header = it->second;
          navigation_loop.Quit();
        } else if (request.relative_url == "/fetch.html?viaFetch") {
          auto it = request.headers.find("Accept-Encoding");
          if (it != request.headers.end())
            subresource_accept_encoding_header = it->second;
          subresource_loop.Quit();
        }
      }));
  ASSERT_TRUE(server.Start());
  GURL url = server.GetURL("/fetch.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  navigation_loop.Run();
  subresource_loop.Run();

  ASSERT_EQ("gzip, deflate, br, zstd", navigation_accept_encoding_header);
  ASSERT_EQ("gzip, deflate, br, zstd", subresource_accept_encoding_header);

  // Since the server uses local variables.
  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}
