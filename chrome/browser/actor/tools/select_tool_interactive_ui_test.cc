// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

using base::test::TestFuture;

namespace actor {
namespace {

class ActorSelectToolBrowserTest : public ActorToolsTest {
 public:
  ActorSelectToolBrowserTest() = default;
  ~ActorSelectToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

}  // namespace

// Test that if the select tool closes the dropdown menu after it makes the
// selection.
//
// On Mac, the <select> dropdown is drawn as an OS widget. When that widget is
// shown, the UI thread is blocked. See `PopupMenuHelper::ShowPopupMenu()`.
// Disable this test on Mac for now until there is a test-only PopupMenuHelper
// that's not blocking.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectToolCloseDropDownMenu) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::ShowPopupWidgetWaiter new_popup_waiter(
      web_contents(), web_contents()->GetPrimaryMainFrame());

  // Click on the dropdown menu.
  std::unique_ptr<ToolRequest> click_action = MakeClickRequest(
      *active_tab(), gfx::ToFlooredPoint(GetCenterCoordinatesOfElementWithId(
                         web_contents(), "plainSelect")));
  ActResultFuture click_result;
  actor_task().Act(ToRequestList(click_action), click_result.GetCallback());
  ExpectOkResult(click_result);

  new_popup_waiter.Wait();
  ASSERT_FALSE(new_popup_waiter.last_initial_rect().IsEmpty());

  // Wait for the dropdown to close.
  const int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), "#plainSelect").value();
  std::unique_ptr<ToolRequest> select_action =
      MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "beta");
  ActResultFuture select_result;
  actor_task().Act(ToRequestList(select_action), select_result.GetCallback());
  ExpectOkResult(select_result);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetPopupWidgets(web_contents()).empty(); }));
}

}  // namespace actor
