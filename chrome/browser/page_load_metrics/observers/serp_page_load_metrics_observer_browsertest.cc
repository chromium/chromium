// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/serp_page_load_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace {

class MockExtensionTelemetryService
    : public safe_browsing::ExtensionTelemetryService {
 public:
  explicit MockExtensionTelemetryService(Profile* profile)
      : safe_browsing::ExtensionTelemetryService(profile, nullptr) {}
  ~MockExtensionTelemetryService() override = default;

  MOCK_METHOD(void, OnDseSerpLoaded, (), (override));
};

std::unique_ptr<KeyedService> BuildMockExtensionTelemetryService(
    content::BrowserContext* context) {
  return std::make_unique<MockExtensionTelemetryService>(
      static_cast<Profile*>(context));
}

}  // namespace

class SerpPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  SerpPageLoadMetricsObserverBrowserTest() = default;
  ~SerpPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(template_url_service);
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL(
        embedded_test_server()->GetURL("/simple.html?q={searchTerms}").spec());
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }
};

IN_PROC_BROWSER_TEST_F(SerpPageLoadMetricsObserverBrowserTest, NotSerp) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  histogram_tester.ExpectUniqueSample("PageLoad.CommittedPageIsDseSerp", false,
                                      1);
}

IN_PROC_BROWSER_TEST_F(SerpPageLoadMetricsObserverBrowserTest, Serp) {
  base::HistogramTester histogram_tester;
  auto* telemetry_service = static_cast<MockExtensionTelemetryService*>(
      safe_browsing::ExtensionTelemetryServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              browser()->profile(),
              base::BindRepeating(&BuildMockExtensionTelemetryService)));

  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  EXPECT_CALL(*telemetry_service, OnDseSerpLoaded()).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html?q=test")));
  waiter->Wait();

  histogram_tester.ExpectUniqueSample("PageLoad.CommittedPageIsDseSerp", true,
                                      1);
}
