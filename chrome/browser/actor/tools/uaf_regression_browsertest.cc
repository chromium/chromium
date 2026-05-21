// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace actor {

namespace {

using base::test::TestFuture;
using content::ChildFrameAt;
using content::ExecJs;
using content::GetDOMNodeId;
using content::RenderFrameHost;
using ActResultFuture = TestFuture<std::vector<ActionResultWithLatencyInfo>>;

class ActorUafRegressionBrowserTest : public ActorToolsTest {
 public:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetupAbaFrames(const std::string& inner_path) {
    GURL outer_url =
        embedded_https_test_server().GetURL("a.test", "/actor/uaf_outer.html");
    GURL mid_url =
        embedded_https_test_server().GetURL("b.test", "/actor/uaf_mid.html");
    GURL inner_url = embedded_https_test_server().GetURL("a.test", inner_path);

    ASSERT_TRUE(content::NavigateToURL(web_contents(), outer_url));
    content::WaitForLoadStop(web_contents());

    // Set mid iframe src
    ASSERT_TRUE(
        ExecJs(web_contents(),
               content::JsReplace("document.getElementById('mid').src = $1",
                                  mid_url)));
    content::WaitForLoadStop(web_contents());

    RenderFrameHost* mid_rfh = ChildFrameAt(main_frame(), 0);
    ASSERT_TRUE(mid_rfh);

    // Set inner iframe src
    ASSERT_TRUE(ExecJs(
        mid_rfh, content::JsReplace("document.getElementById('inner').src = $1",
                                    inner_url)));
    content::WaitForLoadStop(web_contents());
  }

  RenderFrameHost* GetInnerRfh() {
    RenderFrameHost* mid_rfh = ChildFrameAt(main_frame(), 0);
    if (!mid_rfh) {
      return nullptr;
    }
    return ChildFrameAt(mid_rfh, 0);
  }
};

// Regression test for UAF in Glic actor tools.
// See crbug.com/506150628.
IN_PROC_BROWSER_TEST_F(ActorUafRegressionBrowserTest,
                       ClickTool_HandlesSynchronousFrameDetachment) {
  SetupAbaFrames("/actor/click_tool_uaf_inner.html");
  RenderFrameHost* inner_rfh = GetInnerRfh();
  ASSERT_TRUE(inner_rfh);

  const int32_t target_id = GetDOMNodeId(*inner_rfh, "#target").value();

  std::unique_ptr<ToolRequest> action = MakeClickRequest(*inner_rfh, target_id);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // This should not crash the renderer.
  ASSERT_TRUE(result.Wait());
}

// Regression test for UAF in Glic actor tools.
// See crbug.com/506150628.
IN_PROC_BROWSER_TEST_F(ActorUafRegressionBrowserTest,
                       TypeTool_HandlesSynchronousFrameDetachment) {
  SetupAbaFrames("/actor/type_tool_uaf_inner.html");
  RenderFrameHost* inner_rfh = GetInnerRfh();
  ASSERT_TRUE(inner_rfh);

  const int32_t target_id = GetDOMNodeId(*inner_rfh, "#target").value();

  std::unique_ptr<ToolRequest> action = MakeTypeRequest(
      *inner_rfh, target_id, "hello", /* follow_by_enter= */ false);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // This should not crash the renderer.
  ASSERT_TRUE(result.Wait());
}

// Regression test for UAF in Glic actor tools.
// See crbug.com/506150628.
IN_PROC_BROWSER_TEST_F(ActorUafRegressionBrowserTest,
                       DragAndReleaseTool_HandlesSynchronousFrameDetachment) {
  SetupAbaFrames("/actor/drag_and_release_tool_uaf_inner.html");
  RenderFrameHost* inner_rfh = GetInnerRfh();
  ASSERT_TRUE(inner_rfh);

  gfx::RectF bounds = GetBoundingClientRect(*inner_rfh, "#target");
  gfx::Point from_point = gfx::ToRoundedPoint(bounds.CenterPoint());
  gfx::Point to_point = from_point + gfx::Vector2d(100, 100);

  // Drag from target to somewhere else.
  std::unique_ptr<ToolRequest> action =
      MakeDragAndReleaseRequest(*active_tab(), from_point, to_point);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // This should not crash the renderer.
  ASSERT_TRUE(result.Wait());
}

// Regression test for UAF in Glic actor tools.
// See crbug.com/506377279.
IN_PROC_BROWSER_TEST_F(ActorUafRegressionBrowserTest,
                       SelectTool_HandlesSynchronousFrameDetachment) {
  SetupAbaFrames("/actor/select_tool_uaf_inner.html");
  RenderFrameHost* inner_rfh = GetInnerRfh();
  ASSERT_TRUE(inner_rfh);

  const int32_t target_id = GetDOMNodeId(*inner_rfh, "#s").value();

  std::unique_ptr<ToolRequest> action =
      MakeSelectRequest(*inner_rfh, target_id, "b");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // This should not crash the renderer.
  ASSERT_TRUE(result.Wait());
}

}  // namespace
}  // namespace actor
