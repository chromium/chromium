// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/net_error_diagnostics_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/url_request/url_request_failed_job.h"

namespace content {

namespace {

constexpr char kSearchingForDiagnosisScript[] = R"JS(
const kPattern = '<a href="javascript:diagnoseErrors()"';
function check() {
  console.log(document.body.innerHTML);
  if (document.body.innerHTML.indexOf(kPattern) === -1) {
    return 'NOT FOUND';
  } else {
    return 'FOUND';
  }
}

new Promise((resolve) => {
  if (document.readyState === 'complete') {
    resolve(check());
  } else {
    document.addEventListener('load', () => {
      resolve(check());
    });
  }
})
)JS";

}  // namespace

class NetErrorTabHelperTest : public InProcessBrowserTest {
 public:
  NetErrorTabHelperTest() = default;
  ~NetErrorTabHelperTest() override = default;

  NetErrorTabHelperTest(const NetErrorTabHelperTest&) = delete;
  NetErrorTabHelperTest& operator=(const NetErrorTabHelperTest&) = delete;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    chrome_browser_net::NetErrorTabHelper::set_state_for_testing(
        chrome_browser_net::NetErrorTabHelper::TESTING_DEFAULT);

    host_resolver()->AddRule("mock.http", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    tab_helper_ = chrome_browser_net::NetErrorTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  chrome_browser_net::NetErrorTabHelper& tab_helper() const {
    return *tab_helper_;
  }

 private:
  raw_ptr<chrome_browser_net::NetErrorTabHelper, AcrossTasksDanglingUntriaged>
      tab_helper_ = nullptr;
};

class NetErrorTabHelperWithPrerenderingTest : public NetErrorTabHelperTest {
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
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    NetErrorTabHelperTest::SetUp();
  }

  void SetUpOnMainThread() override {
    NetErrorTabHelperTest::SetUpOnMainThread();

    tab_helper().set_dns_probe_status_snoop_callback_for_testing(
        base::BindRepeating(
            &NetErrorTabHelperWithPrerenderingTest::OnDnsProbeStatusSent,
            base::Unretained(this)));

    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            [](URLLoaderInterceptor::RequestParams* params) { return false; }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

  void OnDnsProbeStatusSent(error_page::DnsProbeStatus dns_probe_status) {
    dns_probe_status_queue_.push_back(dns_probe_status);
  }

  void ClearProbeStatusQueue() { dns_probe_status_queue_.clear(); }

  int pending_probe_status_count() const {
    return dns_probe_status_queue_.size();
  }

 private:
  test::PrerenderTestHelper prerender_helper_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::list<error_page::DnsProbeStatus> dns_probe_status_queue_;
};

// TODO(crbug.com/40786063): Enable this test on macOS after the issue is fixed.
#if BUILDFLAG(IS_MAC)
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

  test::PrerenderHostRegistryObserver registry_observer(*GetWebContents());
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
  EXPECT_FALSE(prerender_helper().GetHostForUrl(error_page_url));
  EXPECT_TRUE(prerender_helper().GetHostForUrl(prerender_url));
  EXPECT_FALSE(pending_probe_status_count());
}

IN_PROC_BROWSER_TEST_F(NetErrorTabHelperWithPrerenderingTest,
                       ShowErrorPagesInPrerender) {
  GURL initial_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Overrides the last committed origin to treat the network error as the same
  // url with the non-opaque origins.
  content::OverrideLastCommittedOrigin(GetWebContents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(initial_url));

  GURL prerender_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  test::PrerenderHostRegistryObserver registry_observer(*GetWebContents());

  // Start prerendering `prerender_url`.
  prerender_helper().AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(host_id);
  test::PrerenderHostObserver host_observer(*GetWebContents(), host_id);

  // PrerenderHost is destroyed by net::ERR_NAME_NOT_RESOLVED and it stops
  // prerendering.
  host_observer.WaitForDestroyed();

  // The prerender host should be destroyed.
  host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_FALSE(host_id);
}

class NetErrorTabHelperWithFencedFrameTest : public NetErrorTabHelperTest {
 public:
  NetErrorTabHelperWithFencedFrameTest() = default;
  ~NetErrorTabHelperWithFencedFrameTest() override = default;

  NetErrorTabHelperWithFencedFrameTest(
      const NetErrorTabHelperWithFencedFrameTest&) = delete;
  NetErrorTabHelperWithFencedFrameTest& operator=(
      const NetErrorTabHelperWithFencedFrameTest&) = delete;

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  // Returns true if the platform has support for a diagnostics tool, and it
  // can be launched from |render_frame_host|.
  std::string WebContentsCanShowDiagnosticsTool(
      RenderFrameHost* render_frame_host) {
    auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host);
    return CanShowNetworkDiagnosticsDialog(web_contents) ? "FOUND"
                                                         : "NOT FOUND";
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(NetErrorTabHelperWithFencedFrameTest,
                       CanRunDiagnosticsDialogOnMainFrame) {
  GURL initial_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  EvalJsResult result = EvalJs(GetWebContents()->GetPrimaryMainFrame(),
                               kSearchingForDiagnosisScript);
  ASSERT_TRUE(result.error.empty());
  EXPECT_EQ(WebContentsCanShowDiagnosticsTool(
                GetWebContents()->GetPrimaryMainFrame()),
            result.ExtractString());
}

IN_PROC_BROWSER_TEST_F(NetErrorTabHelperWithFencedFrameTest,
                       CanRunDiagnosticsDialogOnFencedFrame) {
  GURL fenced_frame_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  RenderFrameHost* inner_fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url,
          net::ERR_NAME_NOT_RESOLVED);
  EvalJsResult result =
      EvalJs(inner_fenced_frame_rfh, kSearchingForDiagnosisScript);
  ASSERT_TRUE(result.error.empty());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS has its own diagnostics extension, which doesn't rely on a
  // browser-initiated dialog.
  EXPECT_EQ("FOUND", result.ExtractString());
#else
  EXPECT_EQ("NOT FOUND", result.ExtractString());
#endif
}

}  // namespace content
