// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace customtabs {
namespace {

constexpr char kHeaderValue[] = "TestApp";
constexpr char kHeaderValue2[] = "TestApp2";

class CustomTabsHeader : public AndroidBrowserTest {
 public:
  explicit CustomTabsHeader(size_t expected_header_count = 5)
      : expected_header_count_(expected_header_count) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    run_loop_ = std::make_unique<base::RunLoop>();
    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          if (request.relative_url == "/favicon.ico" ||
              request.relative_url ==
                  "/android/customtabs/test_window_open.html") {
            return;
          }

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
          if (url_header_values_.size() == expected_header_count_)
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
    if (expected_header_count_ > 5) {
      EXPECT_EQ(url_header_values_["google3.jpg"], kHeaderValue2);
    }
  }

  void AdjustHeaderValueAndMakeNewRequest(content::RenderFrameHost* host) {
    auto* web_contents = content::WebContents::FromRenderFrameHost(host);
    ClientDataHeaderWebContentsObserver::FromWebContents(web_contents)
        ->SetHeader(kHeaderValue2);
    std::ignore = ExecJs(host, "document.images[0].src = 'google3.jpg'");
  }

 private:
  const size_t expected_header_count_;
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

class CustomTabsHeaderPrendering : public CustomTabsHeader {
 public:
  CustomTabsHeaderPrendering()
      : CustomTabsHeader(/*expected_header_count=*/6),
        prerender_helper_(
            base::BindRepeating(&CustomTabsHeaderPrendering::web_contents,
                                base::Unretained(this))) {}
  ~CustomTabsHeaderPrendering() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
    return web_contents;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that prerenders set the CCT header and that they react to when
// a new header is applied as well.
IN_PROC_BROWSER_TEST_F(CustomTabsHeaderPrendering, Prerender) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  const GURL main_url = embedded_test_server()->GetURL(
      "www.google.com", "/android/customtabs/test_window_open.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents, main_url));

  const GURL prerender_url = CreateURL();
  // Loads a page in the prerender.
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(WaitForRenderFrameReady(prerender_rfh));

  // Now adjust the header and make a new request.
  AdjustHeaderValueAndMakeNewRequest(prerender_rfh);

  // Expect the headers are all sent.
  ExpectClientDataHeadersSet();
}

}  // namespace
}  // namespace customtabs
