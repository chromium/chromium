// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using PageLoad = ukm::builders::PageLoad;

class UkmPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  UkmPageLoadMetricsObserverBrowserTest() = default;
  ~UkmPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void NavigateTo(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToOriginPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("origin.com", path)));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

  void VerifyNoUKM() {
    auto entries = ukm_recorder_->GetEntriesByName(PageLoad::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  // Returns the metric values that are recorded for the given |metric_name|.
  std::vector<int64_t> GetUkmMetricEntryValues(
      const std::string& entry_name,
      const std::string& metric_name) const {
    return ukm_recorder_->GetMetricsEntryValues(entry_name, metric_name);
  }

  GURL GetOriginURL(const std::string& path) {
    return embedded_test_server()->GetURL("origin.com", path);
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_None) {
  NavigateToOriginPath("/subresource_loading/index.html");
  NavigateAway();

  ASSERT_THAT(GetUkmMetricEntryValues(
                  PageLoad::kEntryName,
                  PageLoad::kMainFrameResource_RequestHadCookiesName),
              testing::ElementsAre(0));
}

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnNextPageLoad) {
  NavigateToOriginPath("/subresource_loading/set_cookies.html");
  NavigateToOriginPath("/subresource_loading/index.html");
  NavigateAway();

  ASSERT_THAT(GetUkmMetricEntryValues(
                  PageLoad::kEntryName,
                  PageLoad::kMainFrameResource_RequestHadCookiesName),
              testing::UnorderedElementsAre(0, 1));
}

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CookiesOnRedirect) {
  NavigateToOriginPath("/subresource_loading/set_cookies.html");
  NavigateToOriginPath("/subresource_loading/redirect_to_index.html");
  NavigateAway();

  ASSERT_THAT(GetUkmMetricEntryValues(
                  PageLoad::kEntryName,
                  PageLoad::kMainFrameResource_RequestHadCookiesName),
              testing::UnorderedElementsAre(0, 1));
}

namespace {
std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    const GURL& redirect_to,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().spec().find("subresource_loading/redirect_me") !=
      std::string::npos) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", redirect_to.spec());
    return std::move(response);
  }
  return nullptr;
}
}  // namespace

// Regression test for crbug.com/1029959.
IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       MainFrameHadCookies_CrossOriginCookiesOnRedirect) {
  net::EmbeddedTestServer redirect_server(net::EmbeddedTestServer::TYPE_HTTP);
  redirect_server.RegisterRequestHandler(base::BindRepeating(
      &HandleRedirectRequest, GetOriginURL("/subresource_loading/index.html")));
  ASSERT_TRUE(redirect_server.Start());

  NavigateToOriginPath("/subresource_loading/set_cookies.html");

  NavigateTo(redirect_server.GetURL("redirect.com",
                                    "/subresource_loading/redirect_me"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            GetOriginURL("/subresource_loading/index.html"));

  NavigateAway();

  ASSERT_THAT(GetUkmMetricEntryValues(
                  PageLoad::kEntryName,
                  PageLoad::kMainFrameResource_RequestHadCookiesName),
              testing::UnorderedElementsAre(0, 1));
}

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       RecordNothingOnUntrackedPage) {
  NavigateAway();
  NavigateAway();

  EXPECT_TRUE(GetUkmMetricEntryValues(
                  PageLoad::kEntryName,
                  PageLoad::kMainFrameResource_RequestHadCookiesName)
                  .empty());
}

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle, page_load_metrics::NavigationHandleUserData::
                             InitiatorLocation::kBookmarkBar);
}

void AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle) {
  page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
      navigation_handle, page_load_metrics::NavigationHandleUserData::
                             InitiatorLocation::kNewTabPage);
}

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       NavigationHandleUserDataTypeMetrics_BookmarkBar) {
  base::RepeatingCallback<void(content::NavigationHandle&)>
      prerender_navigation_handle_callback =
          base::BindRepeating(&AttachBookmarkBarNavigationHandleUserData);

  browser()->tab_strip_model()->GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("origin.com",
                                         "/subresource_loading/index.html"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/std::move(
          prerender_navigation_handle_callback));
  NavigateAway();

  ASSERT_THAT(
      GetUkmMetricEntryValues(PageLoad::kEntryName,
                              PageLoad::kNavigation_InitiatorLocationName),
      testing::ElementsAre(
          static_cast<int>(page_load_metrics::NavigationHandleUserData::
                               InitiatorLocation::kBookmarkBar)));
}

IN_PROC_BROWSER_TEST_F(UkmPageLoadMetricsObserverBrowserTest,
                       NavigationHandleUserDataTypeMetrics_NewTabPage) {
  base::RepeatingCallback<void(content::NavigationHandle&)>
      prerender_navigation_handle_callback =
          base::BindRepeating(&AttachNewTabPageNavigationHandleUserData);

  browser()->tab_strip_model()->GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("origin.com",
                                         "/subresource_loading/index.html"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/std::move(
          prerender_navigation_handle_callback));
  NavigateAway();

  ASSERT_THAT(
      GetUkmMetricEntryValues(PageLoad::kEntryName,
                              PageLoad::kNavigation_InitiatorLocationName),
      testing::ElementsAre(
          static_cast<int>(page_load_metrics::NavigationHandleUserData::
                               InitiatorLocation::kNewTabPage)));
}

}  // namespace
