// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {

namespace {

class ActorMediaControlToolBrowserTest : public ActorToolsTest {
 public:
  ActorMediaControlToolBrowserTest() = default;
  ~ActorMediaControlToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, NoMedia) {
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeMediaControlRequest(*active_tab(), PauseMedia());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kMediaControlNoMedia);
}

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, PauseAndPlayMedia) {
  const GURL url = embedded_test_server()->GetURL("/actor/media.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Start playback.
  ASSERT_TRUE(content::ExecJs(web_contents(), "play()"));

  // Pause the media.
  ActResultFuture pause_result;
  std::unique_ptr<ToolRequest> pause_request =
      MakeMediaControlRequest(*active_tab(), PauseMedia());
  actor_task().Act(ToRequestList(pause_request), pause_result.GetCallback());
  ExpectOkResult(pause_result);
  EXPECT_EQ("pause", content::EvalJs(web_contents(), "event_log.join(',')"));

  // Play the media.
  ActResultFuture play_result;
  std::unique_ptr<ToolRequest> play_request =
      MakeMediaControlRequest(*active_tab(), PlayMedia());
  actor_task().Act(ToRequestList(play_request), play_result.GetCallback());
  ExpectOkResult(play_result);
  EXPECT_EQ("pause,play",
            content::EvalJs(web_contents(), "event_log.join(',')"));
}

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, SeekMedia) {
  const GURL url = embedded_test_server()->GetURL("/actor/media.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Start playback.
  ASSERT_TRUE(content::ExecJs(web_contents(), "play()"));

  // Seek the media.
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeMediaControlRequest(*active_tab(), SeekMedia(1000000));
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ("seek 1", content::EvalJs(web_contents(), "event_log.join(',')"));
}

}  // namespace
}  // namespace actor
