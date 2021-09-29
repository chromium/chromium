// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/url_request/url_request_failed_job.h"

class NetErrorTabHelperWithPrerenderingTest : public InProcessBrowserTest {
 public:
  NetErrorTabHelperWithPrerenderingTest()
      : prerender_helper_(base::BindRepeating(
            &NetErrorTabHelperWithPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}
  ~NetErrorTabHelperWithPrerenderingTest() override = default;

  NetErrorTabHelperWithPrerenderingTest(
      const NetErrorTabHelperWithPrerenderingTest&) = delete;
  NetErrorTabHelperWithPrerenderingTest& operator=(
      const NetErrorTabHelperWithPrerenderingTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    chrome_browser_net::NetErrorTabHelper::set_state_for_testing(
        chrome_browser_net::NetErrorTabHelper::TESTING_DEFAULT);

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              return false;
            }));

    host_resolver()->AddRule("mock.http", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    tab_helper_ = chrome_browser_net::NetErrorTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());

    tab_helper_->set_dns_probe_status_snoop_callback_for_testing(
        base::BindRepeating(
            &NetErrorTabHelperWithPrerenderingTest::OnDnsProbeStatusSent,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void OnDnsProbeStatusSent(error_page::DnsProbeStatus dns_probe_status) {
    dns_probe_status_queue_.push_back(dns_probe_status);
  }

  void ClearProbeStatusQueue() { dns_probe_status_queue_.clear(); }

  int pending_probe_status_count() const {
    return dns_probe_status_queue_.size();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const chrome_browser_net::NetErrorTabHelper& tab_helper() const {
    return *tab_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::list<error_page::DnsProbeStatus> dns_probe_status_queue_;
  chrome_browser_net::NetErrorTabHelper* tab_helper_{nullptr};
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// TODO(crbug.com/1241506): Enable this test on macOS after the issue is fixed.
#if defined(OS_MAC)
#define MAYBE_ErrorPagesDoNotPrerenderOrTriggerDnsProbeStatuses \
  DISABLED_ErrorPagesDoNotPrerenderOrTriggerDnsProbeStatuses
#else
#define MAYBE_ErrorPagesDoNotPrerenderOrTriggerDnsProbeStatuses \
  ErrorPagesDoNotPrerenderOrTriggerDnsProbeStatuses
#endif
IN_PROC_BROWSER_TEST_F(
    NetErrorTabHelperWithPrerenderingTest,
    MAYBE_ErrorPagesDoNotPrerenderOrTriggerDnsProbeStatuses) {
  GURL initial_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  EXPECT_FALSE(pending_probe_status_count());

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());
  GURL error_page_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_page_url));
  EXPECT_TRUE(pending_probe_status_count());

  ClearProbeStatusQueue();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  EXPECT_FALSE(pending_probe_status_count());

  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  // Adding two pages and making sure that only one of them gets pre-rendered.
  prerender_helper().AddPrerenderAsync(error_page_url);
  prerender_helper().AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);
  EXPECT_EQ(content::RenderFrameHost::kNoFrameTreeNodeId,
            prerender_helper().GetHostForUrl(error_page_url));
  EXPECT_NE(content::RenderFrameHost::kNoFrameTreeNodeId,
            prerender_helper().GetHostForUrl(prerender_url));
  EXPECT_FALSE(pending_probe_status_count());
}

IN_PROC_BROWSER_TEST_F(NetErrorTabHelperWithPrerenderingTest,
                       ShowErrorPagesInPrerender) {
  GURL initial_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Overrides the last committed origin to treat the network error as the same
  // url with the non-opaque origins.
  content::OverrideLastCommittedOrigin(GetWebContents()->GetMainFrame(),
                                       url::Origin::Create(initial_url));

  GURL prerender_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());

  // Start prerendering `prerender_url`.
  prerender_helper().AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);

  int_fast64_t host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_NE(content::RenderFrameHost::kNoFrameTreeNodeId, host_id);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);

  // PrerenderHost is destroyed by net::ERR_NAME_NOT_RESOLVED and it stops
  // prerendering.
  host_observer.WaitForDestroyed();

  // The prerender host should be destroyed.
  host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(content::RenderFrameHost::kNoFrameTreeNodeId, host_id);
}
