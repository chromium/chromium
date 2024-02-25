// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServeSimpleHtmlPage(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head><title>Controlled Frame Test</title></head>"
      "<body>A web page to test the Controlled Frame API availability.</body>"
      "</html>");

  return http_response;
}

bool ControlledFrameElementCreated(content::WebContents* web_contents) {
  return content::EvalJs(web_contents,
                         "'src' in document.createElement('controlledframe')")
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
class WebKioskControlledFrameTest
    : public WebKioskBaseTest,
      public testing::WithParamInterface</*use_https=*/bool> {
 public:
  WebKioskControlledFrameTest()
      : web_app_server_(UseHttpsUrl()
                            ? net::test_server::EmbeddedTestServer::TYPE_HTTPS
                            : net::test_server::EmbeddedTestServer::TYPE_HTTP) {
    feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

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

  static bool UseHttpsUrl() { return GetParam(); }

 private:
  void InitAppServer() {
    web_app_server_.RegisterRequestHandler(
        base::BindRepeating(&ServeSimpleHtmlPage));
    ASSERT_TRUE(web_app_handle_ = web_app_server_.StartAndReturnHandle());
  }

  net::test_server::EmbeddedTestServer web_app_server_;
  net::test_server::EmbeddedTestServerHandle web_app_handle_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebKioskControlledFrameTest, ApiAvailability) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  WaitForDocumentLoaded(web_contents);

  // Controlled Frame API should be available for https urls, but not for http
  bool is_api_available = ControlledFrameElementCreated(web_contents);
  if (UseHttpsUrl()) {
    EXPECT_TRUE(is_api_available);
  } else {
    EXPECT_FALSE(is_api_available);
  }
}

INSTANTIATE_TEST_SUITE_P(All, WebKioskControlledFrameTest, testing::Bool());

}  // namespace ash
