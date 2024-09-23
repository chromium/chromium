// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using ::chrome_browser_interstitials::IsShowingSSLInterstitial;
using ::content::RenderFrameHost;
using ::content::WebContents;

namespace {

std::unique_ptr<net::EmbeddedTestServer> CreateExpiredCertServer(
    const base::FilePath& data_dir) {
  auto server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  server->AddDefaultHandlers(data_dir);
  server->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  return server;
}

}  // namespace

class SSLFencedFrameBrowserTest : public InProcessBrowserTest {
 public:
  SSLFencedFrameBrowserTest() = default;
  ~SSLFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  RenderFrameHost* primary_main_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  Browser* InstallAndOpenTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";

    Profile* profile = browser()->profile();

    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
    return app_browser;
  }
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Test that intersitials shouldn't show by any SSL errors from the fenced
// frames.
IN_PROC_BROWSER_TEST_F(SSLFencedFrameBrowserTest, CertErrorInFencedFrame) {
  auto expired_server = CreateExpiredCertServer(GetChromeTestDataDir());
  ASSERT_TRUE(expired_server->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/fenced_frames/basic.html")));

  // Create a fenced frame and navigate to a page with a certificate error.
  const GURL fenced_frame_url =
      expired_server->GetURL("/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      primary_main_frame_host(), fenced_frame_url, net::ERR_CERT_DATE_INVALID);
  ASSERT_NE(nullptr, fenced_frame_rfh);

  EXPECT_FALSE(IsShowingSSLInterstitial(web_contents()));
}

// Test that fenced frames ignore certificate errors that the user ignored in a
// main frame in the past. Interstitals shouldn't be shown otherwise it does
// for the primary page navigation in the app.
IN_PROC_BROWSER_TEST_F(SSLFencedFrameBrowserTest,
                       InAppTestProceededBadCertPageInFencedFrame) {
  auto expired_server = CreateExpiredCertServer(GetChromeTestDataDir());
  ASSERT_TRUE(expired_server->Start());

  // Navigate to a page with a certificate error, and click through the
  // interstitial so the certificate is allowlisted. A fenced frame in this
  // test will be navigated to this url.
  const GURL allow_url = expired_server->GetURL("/fenced_frames/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allow_url));
  EXPECT_TRUE(IsShowingSSLInterstitial(web_contents()));
  const std::string javascript =
      "window.certificateErrorPageController.proceed();";
  ASSERT_TRUE(ExecJs(web_contents(), javascript));

  Browser* app_browser = InstallAndOpenTestWebApp(
      embedded_test_server()->GetURL("/fenced_frames/title2.html"));
  WebContents* app_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsShowingSSLInterstitial(app_contents));

  // Create a fenced frame and navigate to the allowlisted url.
  RenderFrameHost* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      app_contents->GetPrimaryMainFrame(), allow_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  // Ensure that the interstitial isn't shown for the fenced frame in the app.
  EXPECT_FALSE(IsShowingSSLInterstitial(app_contents));
}
