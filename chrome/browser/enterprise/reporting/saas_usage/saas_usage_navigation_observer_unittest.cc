// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/reporting/reporting_features.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {

class MockSaasUsageReportingController : public SaasUsageReportingController {
 public:
  explicit MockSaasUsageReportingController(PrefService* pref_service)
      : SaasUsageReportingController(
            /*local_state_pref_service=*/pref_service,
            /*profile_pref_service=*/pref_service,
            std::make_unique<PrefURLListMatcher>(pref_service, "test_pref"),
            std::make_unique<PrefURLListMatcher>(pref_service, "test_pref")) {}
  MOCK_METHOD(void,
              RecordNavigation,
              (const SaasUsageReportingController::NavigationDataDelegate&),
              (const, override));
};

auto SaasUsageNavigationMatcher(const GURL& url,
                                const std::string& encryption_protocol) {
  return testing::AllOf(
      testing::Property(
          &SaasUsageReportingController::NavigationDataDelegate::GetUrl, url),
      testing::Property(&SaasUsageReportingController::NavigationDataDelegate::
                            GetEncryptionProtocol,
                        encryption_protocol));
}

auto SaasUsageNavigationMatcher(const GURL& url) {
  return testing::Property(
      &SaasUsageReportingController::NavigationDataDelegate::GetUrl, url);
}

}  // namespace

class SaasUsageNavigationObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    testing_pref_service_.registry()->RegisterListPref("test_pref");

    SaasUsageReportingControllerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &SaasUsageNavigationObserverTest::BuildMockController,
                       base::Unretained(this)));
    saas_usage_navigation_observer_ =
        std::make_unique<SaasUsageNavigationObserver>(web_contents());
  }

  void TearDown() override {
    DeleteContents();
    saas_usage_navigation_observer_.reset();
    SaasUsageReportingControllerFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<KeyedService> BuildMockController(
      content::BrowserContext* context) {
    return std::make_unique<MockSaasUsageReportingController>(
        &testing_pref_service_);
  }

  MockSaasUsageReportingController* mock_controller() {
    return static_cast<MockSaasUsageReportingController*>(
        SaasUsageReportingControllerFactory::GetForProfile(profile()));
  }

 private:
  TestingPrefServiceSimple testing_pref_service_;
  std::unique_ptr<SaasUsageNavigationObserver> saas_usage_navigation_observer_;
};

TEST_F(SaasUsageNavigationObserverTest, ReportNavigation) {
  EXPECT_CALL(*mock_controller(), RecordNavigation(SaasUsageNavigationMatcher(
                                      GURL("http://example.com/"))));

  // Navigate
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/"));
}

TEST_F(SaasUsageNavigationObserverTest, ReportReload) {
  EXPECT_CALL(
      *mock_controller(),
      RecordNavigation(SaasUsageNavigationMatcher(GURL("http://example.com/"))))
      .Times(2);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/"));

  // Reload
  content::NavigationSimulator::Reload(web_contents());
}

TEST_F(SaasUsageNavigationObserverTest, DoNotReportSameDocumentNavigation) {
  EXPECT_CALL(
      *mock_controller(),
      RecordNavigation(SaasUsageNavigationMatcher(GURL("http://example.com/"))))
      .Times(1);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/"));

  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);

  // Same document navigation
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://example.com/#foo"), main_rfh());
  simulator->CommitSameDocument();
}

TEST_F(SaasUsageNavigationObserverTest, DoNotReportDownload) {
  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);

  // Simulate a download. A download navigation doesn't commit in the frame
  // it was initiated in and it should not be reported.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://example.com/download.zip"), web_contents());
  simulator->Start();
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader("Content-Disposition",
                     "attachment; filename=download.zip");
  simulator->SetResponseHeaders(headers);
  simulator->ReadyToCommit();
}

TEST_F(SaasUsageNavigationObserverTest, DoNotReportRedirect) {
  EXPECT_CALL(*mock_controller(), RecordNavigation(SaasUsageNavigationMatcher(
                                      GURL("http://example.com/redirect"))))
      .Times(0);
  EXPECT_CALL(*mock_controller(), RecordNavigation(SaasUsageNavigationMatcher(
                                      GURL("http://example.com/final"))))
      .Times(1);

  // Navigate with a redirect. The redirect URL should not be reported.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://example.com/redirect"), web_contents());
  simulator->Start();
  simulator->Redirect(GURL("http://example.com/final"));
  simulator->Commit();
}

TEST_F(SaasUsageNavigationObserverTest, DoNotReportErrorPage) {
  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL("http://example.com/error"), net::ERR_ABORTED);
}

TEST_F(SaasUsageNavigationObserverTest,
       ReportNavigationWithEncryptionProtocol) {
  net::SSLInfo ssl_info;
  net::SSLConnectionStatusSetVersion(
      net::SSLVersion::SSL_CONNECTION_VERSION_QUIC,
      &ssl_info.connection_status);

  EXPECT_CALL(*mock_controller(), RecordNavigation(SaasUsageNavigationMatcher(
                                      GURL("https://example.com/"), "QUIC")))
      .Times(1);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  simulator->SetSSLInfo(ssl_info);
  simulator->Commit();
}

TEST_F(SaasUsageNavigationObserverTest, DoNotReportIframeNavigation) {
  // The first navigation of the main frame.
  EXPECT_CALL(
      *mock_controller(),
      RecordNavigation(SaasUsageNavigationMatcher(GURL("http://example.com/"))))
      .Times(1);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://example.com/"));
  testing::Mock::VerifyAndClearExpectations(mock_controller());

  // Create an iframe and navigate it.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("iframe");

  EXPECT_CALL(
      *mock_controller(),
      RecordNavigation(SaasUsageNavigationMatcher(GURL("http://iframe.com/"))))
      .Times(0);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://iframe.com/"), subframe);
}

}  // namespace enterprise_reporting
