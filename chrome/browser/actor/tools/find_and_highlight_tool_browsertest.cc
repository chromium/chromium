// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/find_and_highlight_tool.h"

#include <memory>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tab_annotation_manager.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {
namespace {

class ActorFindAndHighlightToolBrowserTest : public ActorToolsTest {
 public:
  ActorFindAndHighlightToolBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kProgrammaticScrollAnimationOverride,
        {{"glic_smooth_scroll_threshold_in_dips", "0"}});
  }
  ~ActorFindAndHighlightToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest, FindAndHighlight) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeFindAndHighlightRequest(*active_tab(), "simple");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  auto* annotation_manager =
      TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_TRUE(annotation_manager->HasActiveHighlight());
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_TextNotFound) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeFindAndHighlightRequest(*active_tab(), "nonexistent text");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFindAndHighlightTextNotFound);

  auto* annotation_manager =
      TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_FALSE(annotation_manager->HasActiveHighlight());
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_EmptyQuery) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeFindAndHighlightRequest(*active_tab(), "");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_TabWentAway) {
  tabs::TabHandle stale_handle(99999);
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<FindAndHighlightToolRequest>(stale_handle, "simple");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_ScrollsIntoView) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(0, content::EvalJs(web_contents(), "window.scrollY"));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request = MakeFindAndHighlightRequest(
      *active_tab(), "overflow:auto scrollable offscreen");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents(), "window.scrollY").ExtractDouble() >
           0.0;
  }));

  auto* annotation_manager =
      TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_TRUE(annotation_manager->HasActiveHighlight());
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_ReplaceActiveHighlight) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(0, content::EvalJs(web_contents(), "window.scrollY"));

  // Highlight text near the top of the page first.
  double scroll_y_first = 0.0;
  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request = MakeFindAndHighlightRequest(
        *active_tab(), "overflow:hidden not scrollable");
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectOkResult(result);
    scroll_y_first =
        content::EvalJs(web_contents(), "window.scrollY").ExtractDouble();
  }

  auto* annotation_manager =
      TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_TRUE(annotation_manager->HasActiveHighlight());

  // Replace with offscreen text, which scrolls the page into view.
  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request = MakeFindAndHighlightRequest(
        *active_tab(), "overflow:auto scrollable offscreen");
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(web_contents(), "window.scrollY").ExtractDouble() >
             scroll_y_first;
    }));
  }

  EXPECT_TRUE(annotation_manager->HasActiveHighlight());
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeFindAndHighlightRequest(*active_tab(), "simple");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(actor_task().GetTabs().size(), 1ul);
  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(ActorFindAndHighlightToolBrowserTest,
                       FindAndHighlight_TabNavigatesDuringRequest) {
  const GURL url1 = embedded_test_server()->GetURL("/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url1));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeFindAndHighlightRequest(*active_tab(), "simple");
  actor_task().Act(ToRequestList(request), result.GetCallback());

  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url2));

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFindAndHighlightTextNotFound);

  auto* annotation_manager =
      TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_FALSE(annotation_manager->HasActiveHighlight());
}

}  // namespace
}  // namespace actor
