// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_screenshot_tool.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/get_screenshot_tool_request.h"
#include "chrome/browser/actor/tools/get_screenshot_tool_result.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace actor {
namespace {

class ActorGetScreenshotToolBrowserTest : public ActorToolsTest {
 public:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest, GetScreenshot) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::WaitForCopyableViewInWebContents(web_contents());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  ASSERT_EQ(result.Get().size(), 1u);
  const ToolResult& tool_result = result.Get()[0].tool_result;
  EXPECT_FALSE(
      tool_result.deprecated_action_result()->requires_page_stabilization);
  ASSERT_NE(tool_result.data(), nullptr);

  const GetScreenshotToolResult* screenshot =
      tool_result.data()->As<GetScreenshotToolResult>();
  ASSERT_NE(screenshot, nullptr);
  EXPECT_EQ(screenshot->mime_type, "image/jpeg");

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot->image_data);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
  EXPECT_EQ(screenshot->dimensions, gfx::Size(bitmap.width(), bitmap.height()));
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest, TabWentAway) {
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<GetScreenshotToolRequest>(
          ActorSurfaceHandle::CreateForTesting());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest, DisallowedScheme) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("chrome://version")));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kActionsBlockedForScheme);
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest, CrashedTab) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::CrashTab(web_contents());
  ASSERT_TRUE(web_contents()->IsCrashed());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kRendererCrashed);
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest,
                       PageChangedBeforeToolCreation) {
  const GURL url1 =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url1));

  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url2));

  ActResultFuture result;
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScreenshotCapturePageChanged);
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest,
                       PageChangedDuringCapture) {
  const GURL url1 =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url1));
  content::WaitForCopyableViewInWebContents(web_contents());

  base::test::TestFuture<mojom::ActionResultPtr> add_tab_result;
  actor_task().AddTab(active_tab()->GetHandle(), /*stop_task_on_detach=*/true,
                      add_tab_result.GetCallback());
  ASSERT_TRUE(IsOk(*add_tab_result.Get()));

  content::TestNavigationManager nav_manager(web_contents(), url2);
  content::NavigationController::LoadURLParams params(url2);
  web_contents()->GetController().LoadURLWithParams(params);
  ASSERT_TRUE(nav_manager.WaitForResponse());

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return execution_engine().state() == ExecutionEngine::State::kToolInvoke;
  }));

  nav_manager.ResumeNavigation();
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScreenshotCapturePageChanged);
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest,
                       CrashedDuringCapture) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::WaitForCopyableViewInWebContents(web_contents());

  content::RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher exit_watcher(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  execution_engine().set_tool_invoke_callback_for_testing(
      base::BindLambdaForTesting(
          [&]() { process->Shutdown(content::RESULT_CODE_KILLED); }));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  actor_task().Act(ToRequestList(request), result.GetCallback());

  // Whether the capture finishes, fails, or sees the crash first depends on
  // timing, so this only checks that the tool replies.
  EXPECT_TRUE(result.Wait());
  exit_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ActorGetScreenshotToolBrowserTest,
                       CancelledDuringCapture) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::WaitForCopyableViewInWebContents(web_contents());

  std::unique_ptr<ToolRequest> request =
      MakeGetScreenshotRequest(*active_tab());
  ToolRequest::CreateToolResult create_result =
      request->CreateTool(actor_task().id(), execution_engine());
  ASSERT_TRUE(IsOk(*create_result.result));
  ASSERT_NE(create_result.tool, nullptr);

  base::test::TestFuture<ToolResult> invoke_result;
  create_result.tool->Invoke(invoke_result.GetCallback());
  create_result.tool->Cancel();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(invoke_result.IsReady());
}

}  // namespace
}  // namespace actor
