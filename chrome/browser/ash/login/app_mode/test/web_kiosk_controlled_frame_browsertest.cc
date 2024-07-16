// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/version_info.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

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

class WebKioskControlledFrameBaseTest : public WebKioskBaseTest {
 public:
  WebKioskControlledFrameBaseTest() = delete;

  void SetUpOnMainThread() override {
    InitAppServer();
    SetAppInstallUrl(web_app_server_.base_url().spec());
    WebKioskBaseTest::SetUpOnMainThread();
  }

 protected:
  WebKioskControlledFrameBaseTest(bool feature_enabled,
                                  version_info::Channel channel,
                                  bool https)
      : feature_enabled_(feature_enabled),
        channel_(channel),
        https_(https),
        web_app_server_(UseHttpsUrl()
                            ? net::test_server::EmbeddedTestServer::TYPE_HTTPS
                            : net::test_server::EmbeddedTestServer::TYPE_HTTP) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kIsolatedWebApps};
    std::vector<base::test::FeatureRef> disabled_features;
    if (feature_enabled_) {
      enabled_features.push_back(blink::features::kControlledFrame);
    } else {
      disabled_features.push_back(blink::features::kControlledFrame);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool UseHttpsUrl() { return https_; }

  content::WebContents* TestSetup() {
    InitializeRegularOnlineKiosk();
    SelectFirstBrowser();

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    content::WebContents* web_contents =
        browser_view ? browser_view->GetActiveWebContents() : nullptr;

    WaitForDocumentLoaded(web_contents);
    return web_contents;
  }

 protected:
  bool feature_enabled_{true};

 private:
  void InitAppServer() {
    web_app_server_.RegisterRequestHandler(
        base::BindRepeating(&ServeSimpleHtmlPage));
    ASSERT_TRUE(web_app_handle_ = web_app_server_.StartAndReturnHandle());
  }

  extensions::ScopedCurrentChannel channel_{version_info::Channel::CANARY};
  bool https_{true};

  net::test_server::EmbeddedTestServer web_app_server_;
  net::test_server::EmbeddedTestServerHandle web_app_handle_;

  base::test::ScopedFeatureList feature_list_;
};

class WebKioskControlledFrameHttpTest
    : public WebKioskControlledFrameBaseTest,
      public testing::WithParamInterface</*use_https=*/bool> {
 public:
  WebKioskControlledFrameHttpTest()
      : WebKioskControlledFrameBaseTest(
            /*feature_enabled=*/true,
            /*channel=*/version_info::Channel::CANARY,
            /*https=*/GetParam()) {}
};

IN_PROC_BROWSER_TEST_P(WebKioskControlledFrameHttpTest, ApiAvailability) {
  content::WebContents* web_contents = TestSetup();
  ASSERT_NE(web_contents, nullptr);

  // Controlled Frame API should be available for https urls, but not for http
  bool is_api_available = ControlledFrameElementCreated(web_contents);
  if (feature_enabled_ && UseHttpsUrl()) {
    EXPECT_TRUE(is_api_available);
  } else {
    EXPECT_FALSE(is_api_available);
  }
}

INSTANTIATE_TEST_SUITE_P(All, WebKioskControlledFrameHttpTest, testing::Bool());

class WebKioskControlledFrameChannelTest
    : public WebKioskControlledFrameBaseTest,
      public testing::WithParamInterface<
          std::tuple<bool, version_info::Channel>> {
 public:
  WebKioskControlledFrameChannelTest()
      : WebKioskControlledFrameBaseTest(
            /*feature_enabled=*/std::get<0>(GetParam()),
            /*channel=*/std::get<1>(GetParam()),
            /*https=*/true) {}
};

IN_PROC_BROWSER_TEST_P(WebKioskControlledFrameChannelTest, ApiAvailability) {
  content::WebContents* web_contents = TestSetup();
  ASSERT_NE(web_contents, nullptr);

  // Controlled Frame API should be available for non-stable / non-beta.
  // This works because the mechanism for checking the channel runs using
  // extensions-based code.
  bool is_api_available = ControlledFrameElementCreated(web_contents);
  if (feature_enabled_ &&
      extensions::GetCurrentChannel() != version_info::Channel::STABLE &&
      extensions::GetCurrentChannel() != version_info::Channel::BETA) {
    EXPECT_TRUE(is_api_available);
  } else {
    EXPECT_FALSE(is_api_available);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    WebKioskControlledFrameChannelTest,
    testing::Combine(
        /*feature_enabled=*/testing::Values(true),
        /*channel=*/testing::Values(version_info::Channel::STABLE,
                                    version_info::Channel::BETA,
                                    version_info::Channel::DEV,
                                    version_info::Channel::CANARY,
                                    version_info::Channel::DEFAULT)));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    WebKioskControlledFrameChannelTest,
    testing::Combine(
        /*feature_enabled=*/testing::Values(false),
        /*channel=*/testing::Values(version_info::Channel::STABLE,
                                    version_info::Channel::CANARY)));

}  // namespace ash
