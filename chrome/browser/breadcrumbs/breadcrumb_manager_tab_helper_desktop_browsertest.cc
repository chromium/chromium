// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "base/containers/circular_deque.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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

const base::circular_deque<std::string>& GetEvents() {
  return breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
}

}  // namespace

// Test fixture for BreadcrumbManagerTabHelper class.
class BreadcrumbManagerTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    BreadcrumbManagerTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

// Tests download navigation.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerTabHelperBrowserTest, Download) {
  const size_t num_startup_breadcrumbs = GetEvents().size();

  const GURL url =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("downloads"),
                                base::FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::DownloadURL(browser(), url);

  const auto& events = GetEvents();
  // Breadcrumbs should have been logged for starting and finishing the
  // navigation, and the navigation should be labeled as a download.
  ASSERT_EQ(2u, events.size() - num_startup_breadcrumbs);
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
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    BreadcrumbManagerTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
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

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Broken authentication.
IN_PROC_BROWSER_TEST_F(BreadcrumbManagerTabHelperSecurityStateBrowserTest,
                       BrokenAuthentication) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/google.html")));

  // The breadcrumb event for broken authentication should have been logged.
  auto events = GetEvents();
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
