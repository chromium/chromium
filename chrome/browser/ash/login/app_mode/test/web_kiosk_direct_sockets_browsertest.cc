// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServeSimpleHtmlPage(
    const net::test_server::HttpRequest& /*request*/) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head><title>Direct Sockets Test</title></head>"
      "<body>A web page to test the Direct Sockets API availability.</body>"
      "</html>");

  return http_response;
}

bool IsJsObjectDefined(content::WebContents* web_contents,
                       const std::string& object_name) {
  return content::EvalJs(web_contents, base::ReplaceStringPlaceholders(
                                           "typeof $1 !== 'undefined'",
                                           {object_name}, nullptr))
      .ExtractBool();
}

void WaitForDocumentLoaded(content::WebContents* web_contents) {
  ash::test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents) {
            return content::EvalJs(web_contents,
                                   "document.readyState === 'complete'")
                .ExtractBool();
          },
          web_contents))
      .Wait();
}

}  // namespace

namespace ash {

// TODO(b/326181857): Impl a common parent fixture for api availability tests.
class WebKioskDirectSocketsTest : public WebKioskBaseTest {
 public:
  void SetUpOnMainThread() override {
    InitAppServer();
    SetAppInstallUrl(web_app_server_.base_url().spec());
    WebKioskBaseTest::SetUpOnMainThread();
  }

 protected:
  content::WebContents* GetKioskAppWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

 private:
  void InitAppServer() {
    web_app_server_.RegisterRequestHandler(
        base::BindRepeating(&ServeSimpleHtmlPage));
    ASSERT_TRUE(web_app_handle_ = web_app_server_.StartAndReturnHandle());
  }

  net::test_server::EmbeddedTestServer web_app_server_;
  net::test_server::EmbeddedTestServerHandle web_app_handle_;

  base::test::ScopedFeatureList feature_list_{blink::features::kDirectSockets};
};

// TODO(crbug.com/355290700): Re-enable this test
IN_PROC_BROWSER_TEST_F(WebKioskDirectSocketsTest, DISABLED_ApiAvailability) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  WaitForDocumentLoaded(web_contents);

  EXPECT_TRUE(IsJsObjectDefined(web_contents, "UDPSocket"));
  EXPECT_TRUE(IsJsObjectDefined(web_contents, "TCPSocket"));
  EXPECT_TRUE(IsJsObjectDefined(web_contents, "TCPServerSocket"));
}

}  // namespace ash
