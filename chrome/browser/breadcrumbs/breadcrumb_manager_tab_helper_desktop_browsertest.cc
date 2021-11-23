// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace {

// A WebContentsObserver to allow waiting on a change in visible security state.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  SecurityStyleTestObserver(const SecurityStyleTestObserver&) = delete;
  SecurityStyleTestObserver& operator=(const SecurityStyleTestObserver&) =
      delete;

  ~SecurityStyleTestObserver() override = default;

  void DidChangeVisibleSecurityState() override { run_loop_.Quit(); }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Test fixture for BreadcrumbManagerTabHelper class.
class BreadcrumbManagerTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    BreadcrumbManagerTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    breadcrumb_service_ =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
            browser()->profile());
  }

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_;
};

// Tests download navigation.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerTabHelperBrowserTest, Download) {
  ASSERT_EQ(0ul,
            breadcrumb_service_->GetEvents(/*event_count_limit=*/0).size());

  const GURL url =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("downloads"),
                                base::FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::DownloadURL(browser(), url);

  const std::list<std::string> events =
      breadcrumb_service_->GetEvents(/*event_count_limit=*/0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDownload))
      << events.back();
}

// Tests changes in security states.
class BreadcrumbManagerTabHelperSecurityStateBrowserTest
    : public CertVerifierBrowserTest {
 public:
  BreadcrumbManagerTabHelperSecurityStateBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    BreadcrumbManagerTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    breadcrumb_service_ =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  void SetUpMockCertVerifierForHttpsServer(net::CertStatus cert_status,
                                           int net_result) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;
    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Mixed content.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerTabHelperSecurityStateBrowserTest,
                       MixedContent) {
  ASSERT_EQ(0ul,
            breadcrumb_service_->GetEvents(/*event_count_limit=*/0).size());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Normal HTTPS navigation; no change in security state should be logged.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/title1.html")));
  auto events = breadcrumb_service_->GetEvents(/*event_count_limit=*/0);
  EXPECT_EQ(std::string::npos,
            events.back().find(
                breadcrumbs::kBreadcrumbDidChangeVisibleSecurityState));

  // Append insecure content to the HTTPS navigation.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     "var i = document.createElement('img');"
                                     "i.src = 'http://example.test';"
                                     "document.body.appendChild(i);"));
  observer.WaitForDidChangeVisibleSecurityState();

  // The breadcrumb event for mixed content should have been logged.
  events = breadcrumb_service_->GetEvents(/*event_count_limit=*/0);
  EXPECT_NE(std::string::npos,
            events.back().find(
                breadcrumbs::kBreadcrumbDidChangeVisibleSecurityState));
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbMixedContent))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbAuthenticationBroken))
      << events.back();
}

// Broken authentication.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerTabHelperSecurityStateBrowserTest,
                       BrokenAuthentication) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));

  // The breadcrumb event for broken authentication should have been logged.
  auto events = breadcrumb_service_->GetEvents(/*event_count_limit=*/0);
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded));
  events.pop_back();
  EXPECT_NE(std::string::npos,
            events.back().find(
                breadcrumbs::kBreadcrumbDidChangeVisibleSecurityState));
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbAuthenticationBroken))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbMixedContent))
      << events.back();
}
