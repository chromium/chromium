// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/preloading/preview/preview_manager.h"
#include "chrome/browser/preloading/preview/preview_test_util.h"
#include "chrome/browser/preloading/preview/preview_zoom_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mojo_capability_control_test_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewBrowserTest : public InProcessBrowserTest {
 public:
  PreviewBrowserTest()
      : helper_(std::make_unique<test::PreviewTestHelper>(
            base::BindRepeating(&PreviewBrowserTest::web_contents,
                                base::Unretained(this)))) {}

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);

    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &PreviewBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &PreviewBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_->SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
  }

  GURL GetURL(const std::string& path) {
    return https_server_->GetURL("a.test", path);
  }

  net::test_server::HttpRequest::HeaderMap GetObservedRequestHeadersFor(
      const GURL& url) {
    base::AutoLock auto_lock(lock_);
    std::string path = url.PathForRequest();
    return request_headers_by_path_[path];
  }

  net::EmbeddedTestServer& https_server() { return *https_server_.get(); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  test::PreviewTestHelper& helper() { return *helper_.get(); }
  base::HistogramTester& histogram_tester() { return *histogram_tester_.get(); }

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    ASSERT_FALSE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    request_headers_by_path_.emplace(request.GetURL().PathForRequest(),
                                     request.headers);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  base::Lock lock_;
  std::map<std::string, net::test_server::HttpRequest::HeaderMap>
      request_headers_by_path_ GUARDED_BY(lock_);

  std::unique_ptr<test::PreviewTestHelper> helper_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, SecPurposeHeader) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  GURL preview_url = GetURL("/title2.html");
  helper().InitiatePreview(preview_url);
  helper().WaitUntilLoadFinished();

  net::test_server::HttpRequest::HeaderMap headers =
      GetObservedRequestHeadersFor(preview_url);
  auto it = headers.find("Sec-Purpose");
  ASSERT_NE(it, headers.end());
  EXPECT_EQ(it->second, "prefetch;prerender;preview");
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, CancelWhenPrimaryPageChanged) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  helper().InitiatePreview(GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(true,
            content::EvalJs(preview_web_contents, "document.prerendering"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  ASSERT_FALSE(preview_web_contents);
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, PromoteToNewTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  helper().InitiatePreview(GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(true,
            content::EvalJs(preview_web_contents, "document.prerendering"));

  helper().PromoteToNewTab();

  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(false,
            content::EvalJs(preview_web_contents, "document.prerendering"));
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, TrivialSessionHistory) {
  const std::string title1_path = "/title1.html";
  const GURL title1_url = GetURL(title1_path);
  const GURL title2_url = GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), title1_url));

  helper().InitiatePreview(title2_url);
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ(title2_url, preview_web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, preview_web_contents->GetController().GetEntryCount());
  EXPECT_EQ(
      1, EvalJs(preview_web_contents->GetPrimaryMainFrame(), "history.length"));

  ASSERT_EQ(title1_path, EvalJs(preview_web_contents->GetPrimaryMainFrame(),
                                "location = '/title1.html';"));
  helper().WaitUntilLoadFinished();

  EXPECT_EQ(title1_url, preview_web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, preview_web_contents->GetController().GetEntryCount());
  EXPECT_EQ(
      1, EvalJs(preview_web_contents->GetPrimaryMainFrame(), "history.length"));
}

class MojoCapabilityControlTestContentBrowserClient
    : public ChromeContentBrowserClient,
      public content::test::MojoCapabilityControlTestHelper {
 public:
  MojoCapabilityControlTestContentBrowserClient() {
    previous_client_ = content::SetBrowserClientForTesting(this);
  }
  ~MojoCapabilityControlTestContentBrowserClient() override {
    content::SetBrowserClientForTesting(previous_client_);
  }
  MojoCapabilityControlTestContentBrowserClient(
      const MojoCapabilityControlTestContentBrowserClient&) = delete;
  MojoCapabilityControlTestContentBrowserClient& operator=(
      const MojoCapabilityControlTestContentBrowserClient&) = delete;

 private:
  // ChromeContentBrowserClient implementation.
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    if (previous_client_) {
      previous_client_->RegisterBrowserInterfaceBindersForFrame(
          render_frame_host, map);
    }
    RegisterTestBrowserInterfaceBindersForFrame(render_frame_host, map);
  }
  void RegisterMojoBinderPoliciesForPreview(
      content::MojoBinderPolicyMap& policy_map) override {
    RegisterTestMojoBinderPolicies(policy_map);
  }

  raw_ptr<content::ContentBrowserClient> previous_client_;
};

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, MojoCapabilityControl) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;

  const GURL kInitialUrl = GetURL("/title1.html");
  const GURL kPreviewUrl = GetURL("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Open the target page in preview mode.
  helper().InitiatePreview(kPreviewUrl);
  helper().WaitUntilLoadFinished();

  // Gather RenderFrameHosts for the preview tab.
  std::vector<content::RenderFrameHost*> frames;
  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);
  preview_web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) { frames.push_back(rfh); });
  CHECK_EQ(frames.size(), 2U);

  // A barrier closure to wait until a deferred interface is granted on all
  // frames.
  base::RunLoop run_loop;
  auto barrier_closure =
      base::BarrierClosure(frames.size(), run_loop.QuitClosure());

  mojo::RemoteSet<content::mojom::TestInterfaceForDefer> defer_remote_set;
  mojo::RemoteSet<content::mojom::TestInterfaceForGrant> grant_remote_set;
  for (auto* frame : frames) {
    // Try to bind a kDefer interface.
    mojo::Remote<content::mojom::TestInterfaceForDefer> defer_remote;
    test_browser_client.GetInterface(frame,
                                     defer_remote.BindNewPipeAndPassReceiver());
    //  The barrier closure will be called after the deferred interface is
    //  granted.
    defer_remote->Ping(barrier_closure);
    defer_remote_set.Add(std::move(defer_remote));

    // Try to bind a kGrant interface.
    mojo::Remote<content::mojom::TestInterfaceForGrant> grant_remote;
    test_browser_client.GetInterface(frame,
                                     grant_remote.BindNewPipeAndPassReceiver());
    grant_remote_set.Add(std::move(grant_remote));
  }

  // Verify that BrowserInterfaceBrokerImpl defers running binders whose
  // policies are kDefer until the prerendered page is activated.
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), 0U);
  // Verify that BrowserInterfaceBrokerImpl executes kGrant binders immediately.
  EXPECT_EQ(test_browser_client.GetGrantReceiverSetSize(), frames.size());

  // Activate the prerendered page.
  helper().PromoteToNewTab();

  // Wait until the deferred interface is granted on all frames.
  run_loop.Run();
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), frames.size());
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, ErrorOnNonHttpsUrl) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  helper().InitiatePreview(embedded_test_server()->GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ("Can't preview non-HTTPS URL",
            content::EvalJs(preview_web_contents,
                            "document.querySelector('h1 span').textContent")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, ErrorOnRedirectionToNonHttpsUrl) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  GURL http_url = embedded_test_server()->GetURL("/title2.html");
  GURL preview_url = GetURL("/server-redirect?" + http_url.spec());
  helper().InitiatePreview(preview_url);
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ("Can't preview non-HTTPS URL",
            content::EvalJs(preview_web_contents,
                            "document.querySelector('h1 span').textContent")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, ZoomUsageRecordedOnCancel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  helper().InitiatePreview(GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  histogram_tester().ExpectTotalCount("LinkPreview.Experimental.ZoomUsage", 1);
  histogram_tester().ExpectBucketCount(
      "LinkPreview.Experimental.ZoomUsage",
      PreviewZoomController::ZoomUsage::kOnlyDefaultUsed, 1);
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, ZoomUsageRecordedOnPromoteToNewTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("/title1.html")));

  helper().InitiatePreview(GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  helper().PromoteToNewTab();
  // Wait a task destructing PreviewTab.
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount("LinkPreview.Experimental.ZoomUsage", 1);
  histogram_tester().ExpectBucketCount(
      "LinkPreview.Experimental.ZoomUsage",
      PreviewZoomController::ZoomUsage::kOnlyDefaultUsed, 1);
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, ZoomIsDurableForHost) {
  GURL url_a = https_server().GetURL("a.test", "/title1.html");
  GURL url_b = https_server().GetURL("b.test", "/title1.html");

  auto expect = [this](int x, int y) {
    histogram_tester().ExpectTotalCount("LinkPreview.Experimental.ZoomUsage",
                                        x + y);
    histogram_tester().ExpectBucketCount(
        "LinkPreview.Experimental.ZoomUsage",
        PreviewZoomController::ZoomUsage::kOnlyDefaultUsed, x);
    histogram_tester().ExpectBucketCount(
        "LinkPreview.Experimental.ZoomUsage",
        PreviewZoomController::ZoomUsage::kNonDefaultUsed, y);
  };

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  helper().InitiatePreview(url_a);
  helper().WaitUntilLoadFinished();

  // This preview contributes to kNonDefaultUsed.
  helper().GetManager().PreviewZoomControllerForTesting()->Zoom(
      content::PAGE_ZOOM_IN);

  // Navigate out and cancel preview.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  expect(0, 1);

  // This preview will be shown with the default zoom level because host
  // differs.
  helper().InitiatePreview(url_b);
  helper().WaitUntilLoadFinished();

  // Navigate out and cancel preview.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  expect(1, 1);

  // This preview will be shown with zoom-in due to the effect of first preview.
  helper().InitiatePreview(url_a);
  helper().WaitUntilLoadFinished();

  // This preview contributes to kNonDefaultUsed. The next doesn't.
  helper().GetManager().PreviewZoomControllerForTesting()->Zoom(
      content::PAGE_ZOOM_RESET);

  // Navigate out and cancel preview.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  expect(1, 2);

  helper().InitiatePreview(url_a);
  helper().WaitUntilLoadFinished();

  // Navigate out and cancel preview.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));

  expect(2, 2);
}
