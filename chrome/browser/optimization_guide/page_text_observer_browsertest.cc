// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

namespace {

FrameTextDumpResult MakeFrameDump(mojom::TextDumpEvent event,
                                  content::GlobalRenderFrameHostId rfh_id,
                                  bool amp_frame,
                                  int nav_id,
                                  const std::u16string& contents) {
  return FrameTextDumpResult::Initialize(event, rfh_id, amp_frame, nav_id)
      .CompleteWithContents(contents);
}

class TestConsumer : public PageTextObserver::Consumer {
 public:
  TestConsumer() = default;
  ~TestConsumer() = default;

  void Reset() { was_called_ = false; }

  void PopulateRequest(uint32_t max_size,
                       const std::set<mojom::TextDumpEvent>& events,
                       bool request_amp = false) {
    request_ = std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
    request_->max_size = max_size;
    request_->events = events;
    request_->callback =
        base::BindOnce(&TestConsumer::OnGotTextDump, base::Unretained(this));
    request_->dump_amp_subframes = request_amp;
  }

  void WaitForPageText() {
    if (result_) {
      return;
    }

    base::RunLoop run_loop;
    on_page_text_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_called() const { return was_called_; }

  PageTextDumpResult* result() { return result_.get(); }

  // PageTextObserver::Consumer:
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  MaybeRequestFrameTextDump(content::NavigationHandle* handle) override {
    was_called_ = true;
    return std::move(request_);
  }

 private:
  void OnGotTextDump(const PageTextDumpResult& result) {
    result_ = std::make_unique<PageTextDumpResult>(result);
    if (on_page_text_closure_) {
      std::move(on_page_text_closure_).Run();
    }
  }

  bool was_called_ = false;
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request_;

  base::OnceClosure on_page_text_closure_;
  std::unique_ptr<PageTextDumpResult> result_ = nullptr;
};

}  // namespace

// This tests code in
// //components/optimization_guide/content/browser/page_text_observer.h, but
// this test is in //chrome because the components browsertests do not fully
// standup a renderer process.

class PageTextObserverBrowserTest : public InProcessBrowserTest {
 public:
  PageTextObserverBrowserTest() = default;
  ~PageTextObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &PageTextObserverBrowserTest::RequestHandler, base::Unretained(this)));
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  PageTextObserver* observer() {
    return PageTextObserver::FromWebContents(web_contents());
  }

 protected:
  std::string dynamic_response_body_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    std::string path_value;

    // This script is render blocking in the HTML, but is intentionally slow.
    // This provides important time between commit and first layout for any text
    // dump requests to make it to the renderer, reducing flakes.
    if (request.GetURL().path() == "/slow-first-layout.js") {
      std::unique_ptr<net::test_server::DelayedHttpResponse> resp =
          std::make_unique<net::test_server::DelayedHttpResponse>(
              base::Milliseconds(1500));
      resp->set_code(net::HTTP_OK);
      resp->set_content_type("application/javascript");
      resp->set_content(std::string());
      return resp;
    }

    // This script is onLoad-blocking in the HTML, but is intentionally slow.
    // This provides important time between first layout and finish load for
    // tests that need it.
    if (request.GetURL().path() == "/slow-add-world-text.js") {
      std::unique_ptr<net::test_server::DelayedHttpResponse> resp =
          std::make_unique<net::test_server::DelayedHttpResponse>(
              base::Milliseconds(500));
      resp->set_code(net::HTTP_OK);
      resp->set_content_type("application/javascript");
      resp->set_content(
          "var p = document.createElement('p'); p.innerHTML = 'world'; "
          "document.body.appendChild(p); ");
      return resp;
    }

    if (request.GetURL().path() == "/dynamic.html") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_OK);
      resp->set_content_type("text/html");
      resp->set_content(dynamic_response_body_);
      return resp;
    }

    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, SimpleCaseNoSubframes) {
  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"hello"),
      }));
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, FirstLayoutAndOnLoad) {
  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  // This test can be flaky (crbug.com/1187264), and it always seems to be
  // caused by the renderer never finishing the page load and is thus outside
  // the control of this feature. Thorough testing shows that this flake does
  // not repeat itself, so running the test an extra time is sufficient.
  for (size_t i = 0; i < 2; i++) {
    TestConsumer first_layout_consumer;
    TestConsumer on_load_consumer;
    observer()->AddConsumer(&first_layout_consumer);
    observer()->AddConsumer(&on_load_consumer);

    first_layout_consumer.PopulateRequest(
        /*max_size=*/1024,
        /*events=*/{mojom::TextDumpEvent::kFirstLayout});
    on_load_consumer.PopulateRequest(
        /*max_size=*/1024,
        /*events=*/{mojom::TextDumpEvent::kFinishedLoad});

    GURL url(embedded_test_server()->GetURL("a.com", "/hello_world.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    ASSERT_TRUE(first_layout_consumer.was_called());
    ASSERT_TRUE(on_load_consumer.was_called());

    first_layout_consumer.WaitForPageText();
    on_load_consumer.WaitForPageText();

    if (observer()->outstanding_requests() > 0) {
      // This is a flake. Reset for the next attempt.
      observer()->RemoveConsumer(&first_layout_consumer);
      observer()->RemoveConsumer(&on_load_consumer);
      // A new navigation should unblock any missing requests, which are then
      // discarded.
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
      continue;
    }

    ASSERT_TRUE(first_layout_consumer.result());

    bool has_first_layout_event = false;
    bool has_finished_load_event = false;
    for (const FrameTextDumpResult& result :
         first_layout_consumer.result()->frame_results()) {
      SCOPED_TRACE(result);

      // These fields are the same for both events.
      EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
                result.rfh_id());
      EXPECT_FALSE(result.amp_frame());
      EXPECT_EQ(
          web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
          result.unique_navigation_id());

      ASSERT_TRUE(result.contents().has_value());
      // The text that is dumped during first layout may or may not include the
      // text that is dynamically added by the test page's JavaScript. Checking
      // for text equality is inherently flaky, and this determinism is not a
      // guarantee that we make to callers.
      if (result.event() == mojom::TextDumpEvent::kFirstLayout) {
        EXPECT_TRUE(base::Contains(*result.contents(), u"hello"));
        has_first_layout_event = true;
      }

      // The finished load event is deterministic in what text should be
      // populated on the page.
      if (result.event() == mojom::TextDumpEvent::kFinishedLoad) {
        EXPECT_EQ(u"hello\n\nworld", *result.contents());
        has_finished_load_event = true;
      }
    }

    EXPECT_TRUE(has_first_layout_event);
    EXPECT_TRUE(has_finished_load_event);
    EXPECT_EQ(*first_layout_consumer.result(), *on_load_consumer.result());
    return;
  }
  FAIL();
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, OOPIFAMPSubframe) {
  if (content::IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    // Isolate b.com so that it is guaranteed to be in a different process.
    content::IsolateOriginsForTesting(
        embedded_test_server(),
        browser()->tab_strip_model()->GetActiveWebContents(), {"b.com"});
  }
  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  GURL url(embedded_test_server()->GetURL("a.com", "/dynamic.html"));
  dynamic_response_body_ = base::StringPrintf(
      "<html><body>"
      "<script type=\"text/javascript\" src=\"/slow-first-layout.js\"></script>"
      "<p>mainframe</p>"
      "<iframe name=\"amp\" src=\"%s\"></iframe>"
      "</body></html>",
      embedded_test_server()->GetURL("b.com", "/amp.html").spec().c_str());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();

  content::GlobalRenderFrameHostId amp_frame_id;
  content::RenderFrameHost* amp_frame = content::FrameMatchingPredicate(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "amp"));
  ASSERT_TRUE(amp_frame);
  amp_frame_id = amp_frame->GetGlobalId();

  ASSERT_TRUE(consumer.result());

  bool has_amp_result = false;
  bool has_mainframe_result = false;
  for (const FrameTextDumpResult& result : consumer.result()->frame_results()) {
    if (result.amp_frame()) {
      EXPECT_EQ(mojom::TextDumpEvent::kFinishedLoad, result.event());
      EXPECT_EQ(amp_frame_id, result.rfh_id());
      EXPECT_EQ(
          web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
          result.unique_navigation_id());
      // Sometimes blink adds trailing whitespace since there's another element
      // on the page. Happens non-deterministically.
      EXPECT_EQ(u"AMP",
                std::u16string(base::TrimWhitespace(
                    *result.contents(), base::TrimPositions::TRIM_ALL)));
      has_amp_result = true;
    } else {
      EXPECT_EQ(mojom::TextDumpEvent::kFirstLayout, result.event());
      EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
                result.rfh_id());
      EXPECT_EQ(
          web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
          result.unique_navigation_id());
      // Sometimes blink adds trailing whitespace since there's another element
      // on the page. Happens non-deterministically.
      EXPECT_EQ(u"mainframe",
                std::u16string(base::TrimWhitespace(
                    *result.contents(), base::TrimPositions::TRIM_ALL)));
      has_mainframe_result = true;
    }
  }
  EXPECT_TRUE(has_amp_result);
  EXPECT_TRUE(has_mainframe_result);
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, OOPIFNotAmpSubframe) {
  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  GURL url(embedded_test_server()->GetURL("a.com", "/dynamic.html"));
  dynamic_response_body_ = base::StringPrintf(
      "<html><body>"
      "<script type=\"text/javascript\" src=\"/slow-first-layout.js\"></script>"
      "<p>mainframe</p>"
      "<iframe src=\"%s\"></iframe>"
      "</body></html>",
      embedded_test_server()->GetURL("b.com", "/hello.html").spec().c_str());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  ASSERT_EQ(1U, consumer.result()->frame_results().size());

  const auto& result = *consumer.result()->frame_results().begin();

  EXPECT_EQ(mojom::TextDumpEvent::kFirstLayout, result.event());
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
            result.rfh_id());
  EXPECT_FALSE(result.amp_frame());
  EXPECT_EQ(web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
            result.unique_navigation_id());
  EXPECT_EQ(u"mainframe", base::TrimWhitespace(*result.contents(),
                                               base::TrimPositions::TRIM_ALL));
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, SameProcessIframe) {
  // Give the browser a moment to startup (helps to reduce flakes by ensuring
  // renderer and browser are ready to go).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("a.com", "/hello.html"))));

  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFinishedLoad});

  GURL url(embedded_test_server()->GetURL("a.com", "/dynamic.html"));
  dynamic_response_body_ = base::StringPrintf(
      "<html><body>"
      "<script type=\"text/javascript\" src=\"/slow-first-layout.js\"></script>"
      "<p>mainframe</p>"
      "<iframe src=\"%s\"></iframe>"
      "</body></html>",
      embedded_test_server()->GetURL("a.com", "/hello.html").spec().c_str());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFinishedLoad,
              web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe\n\nhello"),
      }));
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, SameProcessAMPSubframe) {
  // Give the browser a moment to startup (helps to reduce flakes by ensuring
  // renderer and browser are ready to go).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("a.com", "/hello.html"))));

  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  GURL url(embedded_test_server()->GetURL("a.com", "/dynamic.html"));
  dynamic_response_body_ = base::StringPrintf(
      "<html><body>"
      "<script type=\"text/javascript\" src=\"/slow-first-layout.js\"></script>"
      "<p>mainframe</p>"
      "<iframe src=\"%s\"></iframe>"
      "</body></html>",
      embedded_test_server()->GetURL("a.com", "/amp.html").spec().c_str());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe"),
      }));
}

class PageTextObserverFencedFrameBrowserTest
    : public PageTextObserverBrowserTest {
 public:
  PageTextObserverFencedFrameBrowserTest() = default;
  ~PageTextObserverFencedFrameBrowserTest() override = default;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PageTextObserverFencedFrameBrowserTest,
                       DoNotDispatchResponseOnFencedFrame) {
  base::HistogramTester histogram_tester;

  PageTextObserver::CreateForWebContents(web_contents());

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.AbandonedRequests", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad", 1);

  // Create a fenced frame.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  // Loading a URL in a fenced frame should not increase
  // OptimizationGuide.PageTextDump.AbandonedRequests and
  // OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad count.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.AbandonedRequests", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad", 1);
}

}  // namespace optimization_guide
