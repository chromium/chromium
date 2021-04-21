// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
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
namespace {

constexpr char kHeaderValue[] = "TestApp";

class CustomTabsHeader : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    run_loop_ = std::make_unique<base::RunLoop>();
    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          if (request.relative_url == "/favicon.ico")
            return;

          std::string value;
          auto it = request.headers.find("X-CCT-Client-Data");
          if (it != request.headers.end())
            value = it->second;

          std::string path = request.relative_url;
          base::ReplaceFirstSubstringAfterOffset(&path, 0,
                                                 "/android/customtabs/", "");
          if (base::StartsWith(path, "cct_header.html",
                               base::CompareCase::SENSITIVE)) {
            path = "cct_header.html";
          }

          url_header_values_[path] = value;
          if (url_header_values_.size() == 5)
            run_loop_->Quit();
        }));

    ASSERT_TRUE(embedded_test_server()->Start());

    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    ClientDataHeaderWebContentsObserver::CreateForWebContents(web_contents);
    ClientDataHeaderWebContentsObserver::FromWebContents(web_contents)
        ->SetHeader(kHeaderValue);
  }

  GURL CreateURL() {
    base::StringPairs replacements;
    replacements.push_back(std::make_pair(
        "REPLACE_WITH_HTTP_PORT",
        base::NumberToString(embedded_test_server()->host_port_pair().port())));

    std::string path = net::test_server::GetFilePathWithReplacements(
        "/android/customtabs/cct_header.html", replacements);
    return embedded_test_server()->GetURL("www.google.com", path);
  }

  void ExpectClientDataHeadersSet() {
    run_loop_->Run();
    EXPECT_EQ(url_header_values_["cct_header.html"], kHeaderValue);
    EXPECT_EQ(url_header_values_["cct_header_frame.html"], kHeaderValue);
    EXPECT_EQ(url_header_values_["google1.jpg"], kHeaderValue);
    EXPECT_EQ(url_header_values_["google2.jpg"], kHeaderValue);
    EXPECT_EQ(url_header_values_["non_google.jpg"], "");
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<std::string, std::string> url_header_values_;
};

IN_PROC_BROWSER_TEST_F(CustomTabsHeader, Basic) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_TRUE(content::NavigateToURL(web_contents, CreateURL()));
  ExpectClientDataHeadersSet();
}

IN_PROC_BROWSER_TEST_F(CustomTabsHeader, Popup) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "window.open('" + CreateURL().spec() + "')"));
  ExpectClientDataHeadersSet();
}

}  // namespace
}  // namespace customtabs
