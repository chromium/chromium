// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/ttc/core/page_context.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/session_controller.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ttc {
namespace {

// Returns the first iframe node found in the tree rooted at `node`, or null if
// there isn't one.
const optimization_guide::proto::ContentNode* FindIframeNode(
    const optimization_guide::proto::ContentNode& node) {
  if (node.content_attributes().attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    return &node;
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    if (const optimization_guide::proto::ContentNode* found =
            FindIframeNode(child)) {
      return found;
    }
  }

  return nullptr;
}

// Returns the text of all text nodes in the tree rooted at `node`.
std::vector<std::string> CollectText(
    const optimization_guide::proto::ContentNode& node) {
  std::vector<std::string> text;
  if (node.content_attributes().has_text_data()) {
    text.push_back(node.content_attributes().text_data().text_content());
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    std::vector<std::string> child_text = CollectText(child);
    text.insert(text.end(), std::make_move_iterator(child_text.begin()),
                std::make_move_iterator(child_text.end()));
  }

  return text;
}

class PageContextMonitorBrowserTest : public TtcCoreBrowserTestBase {
 public:
  PageContextMonitorBrowserTest() = default;
  ~PageContextMonitorBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    // Allows tests to load content from different sites.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    if (conversation()) {
      // Release expectations after the test to avoid any uninteresting calls
      // on the mock conversation.
      testing::Mock::VerifyAndClearExpectations(conversation());
    }
    TtcCoreBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  // Starts a session and waits for the initial notification, leaving no
  // expectations set on the conversation.
  void StartSession() {
    ttc_service().StartSession();
    ASSERT_TRUE(conversation());
    EXPECT_TRUE(ExpectPageChange().Wait());
  }

  // Expects the conversation to be notified of a page change exactly once.
  // Callers wait for the notification using Wait() on the returned future.
  base::test::TestFuture<void> ExpectPageChange() {
    base::test::TestFuture<void> future;
    EXPECT_CALL(*conversation(), OnPageContextChanged())
        .WillOnce(base::test::InvokeFuture(future));
    return future;
  }

  // Navigates to an arbitrary page but blocks it from firing the load event.
  void NavigateButBlockLoad() {
    // The test page embeds `subframe_url()` in an iframe, so holding back the
    // subframe's response keeps the page from firing its load event.
    GURL main_url = embedded_test_server()->GetURL("/iframe.html");
    GURL subframe_url = embedded_test_server()->GetURL("/title1.html");
    main_manager_ = std::make_unique<content::TestNavigationManager>(
        web_contents(), main_url);
    subframe_manager_ = std::make_unique<content::TestNavigationManager>(
        web_contents(), subframe_url);

    // Don't use NavigateToURL since it waits for the load to stop, which the
    // held back subframe prevents.
    web_contents()->GetController().LoadURL(main_url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            std::string());
    ASSERT_TRUE(main_manager_->WaitForNavigationFinished());
    ASSERT_TRUE(subframe_manager_->WaitForResponse());
  }

  // Unblocks the load in the navigation started from NavigateButBlockLoad.
  void UnblockLoad() {
    CHECK(subframe_manager_);
    ASSERT_TRUE(subframe_manager_->WaitForNavigationFinished());
  }

 private:
  std::unique_ptr<content::TestNavigationManager> main_manager_;
  std::unique_ptr<content::TestNavigationManager> subframe_manager_;
};

// Starting a session must asynchronously notify the conversation.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, InitialPageChange) {
  ttc_service().StartSession();
  ASSERT_TRUE(conversation());
  EXPECT_TRUE(ExpectPageChange().Wait());
}

// Navigating the monitored page must notify the conversation that the page
// context it has is stale before the load event is fired.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, PrimaryPageNavigated) {
  StartSession();

  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());

  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
}

// Ensure the context monitor signals when a load event is fired.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, LoadStopped) {
  StartSession();

  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // No notification while the page is still loading.
  EXPECT_CALL(*conversation(), OnPageContextChanged).Times(0);
  TinyWait();
  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  auto load_stopped = ExpectPageChange();
  UnblockLoad();
  EXPECT_TRUE(load_stopped.Wait());
}

// Fetching the page context must return the content of the monitored page.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, GetPageContext) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  ttc_service().StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<PageContextResult> future;
  session_controller->GetPageContext(future.GetCallback());

  PageContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->ai_page_content.has_value());

  const optimization_guide::proto::AnnotatedPageContent& page_content =
      result->ai_page_content->proto;
  EXPECT_EQ(page_content.main_frame_data().title(), "OK");

  ASSERT_TRUE(page_content.has_root_node());
  ASSERT_GT(page_content.root_node().children_nodes_size(), 0);
  const auto& first_child = page_content.root_node().children_nodes(0);
  ASSERT_TRUE(first_child.content_attributes().has_text_data());
  EXPECT_EQ(base::TrimWhitespaceASCII(
                first_child.content_attributes().text_data().text_content(),
                base::TRIM_ALL),
            "Non empty simple page");
}

// Fetching the page context must not include the content of a cross-site
// iframe.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       GetPageContextCrossSiteIframe) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/iframe.html")));
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL("b.com", "/simple.html")));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(subframe);
  ASSERT_FALSE(subframe->GetLastCommittedOrigin().IsSameOriginWith(
      main_frame->GetLastCommittedOrigin()));

  ttc_service().StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<PageContextResult> future;
  session_controller->GetPageContext(future.GetCallback());

  PageContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->ai_page_content.has_value());

  const optimization_guide::proto::AnnotatedPageContent& page_content =
      result->ai_page_content->proto;
  EXPECT_EQ(page_content.main_frame_data().title(), "iframe test");
  ASSERT_TRUE(page_content.has_root_node());

  // The iframe must be present but redacted, with none of its content.
  const optimization_guide::proto::ContentNode* iframe =
      FindIframeNode(page_content.root_node());
  ASSERT_TRUE(iframe);
  const optimization_guide::proto::IframeData& iframe_data =
      iframe->content_attributes().iframe_data();
  EXPECT_FALSE(iframe_data.has_frame_data());
  ASSERT_TRUE(iframe_data.has_redacted_frame_metadata());
  EXPECT_EQ(iframe_data.redacted_frame_metadata().reason(),
            optimization_guide::proto::IframeData_RedactedFrameMetadata::
                REASON_CROSS_SITE);
  EXPECT_EQ(iframe->children_nodes_size(), 0);

  // The subframe's text must not appear anywhere in the content.
  EXPECT_THAT(CollectText(page_content.root_node()),
              testing::Not(testing::Contains(
                  testing::HasSubstr("Non empty simple page"))));
}

}  // namespace
}  // namespace ttc
