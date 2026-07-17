// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using base::test::TestFuture;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

class ActorScrollToToolBrowserTest : public ActorToolsTest {
 public:
  ActorScrollToToolBrowserTest() = default;
  ~ActorScrollToToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

class ActorScrollToToolToctouBrowserTest : public ActorScrollToToolBrowserTest {
 public:
  ActorScrollToToolToctouBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kGlicActorToctouValidation);
  }
  ~ActorScrollToToolToctouBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorScrollToToolBrowserTest, FailsOnInvalidNodeID) {
  const GURL url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  std::unique_ptr<ToolRequest> action =
      MakeScrollToRequest(*main_frame(), kNonExistentContentNodeId);

  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollX"));
  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

IN_PROC_BROWSER_TEST_F(ActorScrollToToolBrowserTest, ScrollsToValidNodeID) {
  const GURL url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const gfx::RectF viewport_rect(0, 0, web_contents()->GetSize().width(),
                                 web_contents()->GetSize().height());

  for (std::string_view query : {"#in-viewport", "#out-of-viewport"}) {
    SCOPED_TRACE(query);

    int content_node_id = GetDOMNodeId(*main_frame(), query).value();

    std::unique_ptr<ToolRequest> action =
        MakeScrollToRequest(*main_frame(), content_node_id);
    ActResultFuture result_success;
    actor_task().Act(ToRequestList(action), result_success.GetCallback());
    ExpectOkResult(result_success);

    gfx::RectF rect = GetBoundingClientRect(*main_frame(), query);
    EXPECT_TRUE(viewport_rect.Contains(rect)) << rect.ToString();
  }
}

IN_PROC_BROWSER_TEST_F(ActorScrollToToolBrowserTest,
                       PositionFixed_DoesNotScroll) {
  const GURL url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int content_node_id = GetDOMNodeId(*main_frame(), "#fixed").value();

  std::unique_ptr<ToolRequest> action =
      MakeScrollToRequest(*main_frame(), content_node_id);
  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollX"));
  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

IN_PROC_BROWSER_TEST_F(ActorScrollToToolToctouBrowserTest,
                       ScrollToTool_RequiresTargetInLastApc) {
  // Scroll-to rejects a target added after APC was saved.
  const GURL url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Save APC before adding the target.
  GetPageApc();

  // Put the target below the viewport so an accepted action would scroll the
  // page.
  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    const late_target = document.createElement('div');
    late_target.id = 'late-scroll-to-target';
    late_target.textContent = 'late target';
    late_target.style.cssText = 'margin-top: 3000px; height: 40px;';
    document.body.appendChild(late_target);
  )JS"));

  int content_node_id =
      GetDOMNodeId(*main_frame(), "#late-scroll-to-target").value();
  std::unique_ptr<ToolRequest> action =
      MakeScrollToRequest(*main_frame(), content_node_id);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

IN_PROC_BROWSER_TEST_F(ActorScrollToToolBrowserTest,
                       DisplayNone_DoesNotScroll) {
  const GURL url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int content_node_id = GetDOMNodeId(*main_frame(), "#display-none").value();

  std::unique_ptr<ToolRequest> action =
      MakeScrollToRequest(*main_frame(), content_node_id);
  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementOffscreen);

  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollX"));
  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

}  // namespace
}  // namespace actor
