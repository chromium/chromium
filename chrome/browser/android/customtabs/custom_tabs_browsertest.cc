// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace customtabs {

class CustomTabsHeader : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(CustomTabsHeader, Basic) {
  base::RunLoop run_loop;
  std::map<std::string, std::string> url_header_values;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/favicon.ico")
          return;

        std::string value;
        auto it = request.headers.find("X-CCT-Client-Data");
        if (it != request.headers.end())
          value = it->second;

        std::string path = request.relative_url;
        base::ReplaceFirstSubstringAfterOffset(&path, 0, "/android/customtabs/",
                                               "");
        if (base::StartsWith(path, "cct_header.html",
                             base::CompareCase::SENSITIVE)) {
          path = "cct_header.html";
        }

        url_header_values[path] = value;
        if (url_header_values.size() == 5)
          run_loop.Quit();
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  ClientDataHeaderWebContentsObserver::CreateForWebContents(web_contents);
  ClientDataHeaderWebContentsObserver::FromWebContents(web_contents)
      ->SetHeader("TestApp");

  base::StringPairs replacements;
  replacements.push_back(std::make_pair(
      "REPLACE_WITH_HTTP_PORT",
      base::NumberToString(embedded_test_server()->host_port_pair().port())));

  std::string path = net::test_server::GetFilePathWithReplacements(
      "/android/customtabs/cct_header.html", replacements);
  GURL url = embedded_test_server()->GetURL("www.google.com", path);
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  run_loop.Run();

  EXPECT_EQ(url_header_values["cct_header.html"], "TestApp");
  EXPECT_EQ(url_header_values["cct_header_frame.html"], "TestApp");
  EXPECT_EQ(url_header_values["google1.jpg"], "TestApp");
  EXPECT_EQ(url_header_values["google2.jpg"], "TestApp");
  EXPECT_EQ(url_header_values["non_google.jpg"], "");
}

}  // namespace customtabs
