// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
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

namespace optimization_guide {

class TestConsumer : public PageTextObserver::Consumer {
 public:
  TestConsumer() = default;
  ~TestConsumer() = default;

  void Reset() { was_called_ = false; }

  void PopulateRequest(uint32_t max_size,
                       const std::set<mojom::TextDumpEvent>& events) {
    request_ = std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
    request_->max_size = max_size;
    request_->events = events;
    request_->callback = base::BindRepeating(&TestConsumer::OnGotTextDump,
                                             base::Unretained(this));
  }

  void WaitForPageText() {
    if (text_) {
      return;
    }

    base::RunLoop run_loop;
    on_page_text_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_called() const { return was_called_; }

  const base::Optional<base::string16>& text() const { return text_; }

  // PageTextObserver::Consumer:
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  MaybeRequestFrameTextDump(content::NavigationHandle* handle) override {
    was_called_ = true;
    return std::move(request_);
  }

 private:
  void OnGotTextDump(const base::string16& text) {
    text_ = text;
    if (on_page_text_closure_) {
      std::move(on_page_text_closure_).Run();
    }
  }

  bool was_called_ = false;

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request_;

  base::OnceClosure on_page_text_closure_;

  base::Optional<base::string16> text_;
};

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

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&PageTextObserverBrowserTest::HandleSlowRequest,
                            base::Unretained(this)));
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

 private:
  // This script is render blocking in the HTML, but is intentionally slow. This
  // provides important time between commit and first layout for any text dump
  // requests to make it to the renderer, reducing flakes.
  std::unique_ptr<net::test_server::HttpResponse> HandleSlowRequest(
      const net::test_server::HttpRequest& request) {
    std::string path_value;
    if (request.GetURL().path() == "/slow-first-layout.js") {
      std::unique_ptr<net::test_server::DelayedHttpResponse> resp =
          std::make_unique<net::test_server::DelayedHttpResponse>(
              base::TimeDelta::FromMilliseconds(500));
      resp->set_code(net::HTTP_OK);
      resp->set_content_type("application/javascript");
      resp->set_content(std::string());
      return resp;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, SimpleCase) {
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
  ASSERT_TRUE(consumer.text());
  EXPECT_EQ(base::ASCIIToUTF16("hello"), *consumer.text());
}

IN_PROC_BROWSER_TEST_F(PageTextObserverBrowserTest, FirstLayoutAndOnLoad) {
  PageTextObserver::CreateForWebContents(web_contents());
  ASSERT_TRUE(observer());

  TestConsumer first_layout_consumer;
  observer()->AddConsumer(&first_layout_consumer);
  first_layout_consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  TestConsumer on_load_consumer;
  observer()->AddConsumer(&on_load_consumer);
  on_load_consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFinishedLoad});

  GURL url(embedded_test_server()->GetURL("a.com", "/hello_world.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(first_layout_consumer.was_called());
  ASSERT_TRUE(on_load_consumer.was_called());

  first_layout_consumer.WaitForPageText();
  on_load_consumer.WaitForPageText();

  ASSERT_TRUE(first_layout_consumer.text());
  // This check can be a bit flaky on some platforms. Check that "hello" is
  // present, since the text captured may already be "hello world".
  EXPECT_TRUE(base::Contains(*first_layout_consumer.text(),
                             base::ASCIIToUTF16("hello")));

  ASSERT_TRUE(on_load_consumer.text());
  EXPECT_EQ(base::ASCIIToUTF16("hello\n\nworld"), *on_load_consumer.text());
}

}  // namespace optimization_guide
