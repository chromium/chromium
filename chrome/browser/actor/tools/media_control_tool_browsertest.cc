// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {

namespace {

class ActorMediaControlToolBrowserTest : public ActorToolsTest {
 public:
  ActorMediaControlToolBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor},
        /*disabled_features=*/{});
  }
  ~ActorMediaControlToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, NoMedia) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeMediaControlRequest(*active_tab(), PauseMedia());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kMediaControlNoMedia);
}

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, PauseAndPlayMedia) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/media.html");
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
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('pause')"));

  // Play the media.
  ActResultFuture play_result;
  std::unique_ptr<ToolRequest> play_request =
      MakeMediaControlRequest(*active_tab(), PlayMedia());
  actor_task().Act(ToRequestList(play_request), play_result.GetCallback());
  ExpectOkResult(play_result);
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('play')"));
}

IN_PROC_BROWSER_TEST_F(ActorMediaControlToolBrowserTest, SeekMedia) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/media.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Start playback to initialize media session.
  ASSERT_TRUE(content::ExecJs(web_contents(), "play()"));
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('play')"));

  // Pause it so the currentTime doesn't drift during seek.
  ASSERT_TRUE(content::ExecJs(web_contents(), "video.pause()"));
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('pause')"));

  // Seek the media.
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      MakeMediaControlRequest(*active_tab(), SeekMedia(1000));
  std::unique_ptr<ToolRequest> request_negative_time =
      MakeMediaControlRequest(*active_tab(), SeekMedia(-1000));
  std::unique_ptr<ToolRequest> request_unreachable_time =
      MakeMediaControlRequest(*active_tab(), SeekMedia(10000));
  actor_task().Act(
      ToRequestList(request, request_negative_time, request_unreachable_time),
      result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForSeek(1.0)"));
  EXPECT_EQ(
      1.0,
      content::EvalJs(web_contents(), "video.currentTime").ExtractDouble());
}

}  // namespace
}  // namespace actor
