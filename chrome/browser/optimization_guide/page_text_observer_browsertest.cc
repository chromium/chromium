// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

namespace {

FrameTextDumpResult MakeFrameDump(mojom::TextDumpEvent event,
                                  content::GlobalFrameRoutingId rfh_id,
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

  base::Optional<PageTextDumpResult> result() {
    return base::OptionalFromPtr(result_.get());
  }

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
  std::unique_ptr<PageTextDumpResult> result_;
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
              base::TimeDelta::FromMilliseconds(500));
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
              base::TimeDelta::FromMilliseconds(500));
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
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
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
  //
  // If this test starts timing out or failing the |EXPECT_THAT| check, then
  // that is indicative of a real issue.
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
    ui_test_utils::NavigateToURL(browser(), url);

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
      ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
      continue;
    }

    ASSERT_TRUE(first_layout_consumer.result());
    EXPECT_THAT(
        first_layout_consumer.result()->frame_results(),
        ::testing::UnorderedElementsAreArray({
            MakeFrameDump(
                mojom::TextDumpEvent::kFirstLayout,
                web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
                /*amp_frame=*/false,
                web_contents()
                    ->GetController()
                    .GetVisibleEntry()
                    ->GetUniqueID(),
                u"hello"),
            MakeFrameDump(
                mojom::TextDumpEvent::kFinishedLoad,
                web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
                /*amp_frame=*/false,
                web_contents()
                    ->GetController()
                    .GetVisibleEntry()
                    ->GetUniqueID(),
                u"hello\n\nworld"),
        }));

    EXPECT_EQ(first_layout_consumer.result(), on_load_consumer.result());
    return;
  }
  FAIL();
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, OOPIFAMPSubframe) {
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
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();

  content::GlobalFrameRoutingId amp_frame_id;
  for (auto* rfh : web_contents()->GetMainFrame()->GetFramesInSubtree()) {
    if (rfh->GetFrameName() == "amp") {
      amp_frame_id = rfh->GetGlobalFrameRoutingId();
      break;
    }
  }

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe"),
          MakeFrameDump(
              mojom::TextDumpEvent::kFinishedLoad, amp_frame_id,
              /*amp_frame=*/true,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"AMP"),
      }));
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
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe"),
      }));
}

class PageTextObserverSingleProcessBrowserTest
    : public PageTextObserverBrowserTest {
 public:
  PageTextObserverSingleProcessBrowserTest() = default;
  ~PageTextObserverSingleProcessBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    PageTextObserverBrowserTest::SetUpCommandLine(cmd_line);
    cmd_line->AppendSwitch("single-process");
  }
};

#if defined(OS_MAC)
// https://crbug.com/1189556
#define MAYBE_SameProcessIframe DISABLED_SameProcessIframe
#else
#define MAYBE_SameProcessIframe SameProcessIframe
#endif
IN_PROC_BROWSER_TEST_F(PageTextObserverSingleProcessBrowserTest,
                       MAYBE_SameProcessIframe) {
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
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFinishedLoad,
              web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe\n\nhello"),
      }));
}

IN_PROC_BROWSER_TEST_F(PageTextObserverSingleProcessBrowserTest,
                       SameProcessAMPSubframe) {
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
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout,
              web_contents()->GetMainFrame()->GetGlobalFrameRoutingId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"mainframe"),
      }));
}

}  // namespace optimization_guide
