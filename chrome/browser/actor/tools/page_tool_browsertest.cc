// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

using base::test::TestFuture;
using content::GetDOMNodeId;

namespace actor {

namespace {

void FlushChromeRenderFrameForTesting(content::RenderFrameHost& rfh) {
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  rfh.GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame);
  chrome_render_frame.FlushForTesting();
}

class ActorPageToolBrowserTest : public ActorToolsTest {
 public:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorPageToolBrowserTest, RemovedElement) {
  const GURL url = embedded_test_server()->GetURL("/actor/link.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#link");
  ASSERT_TRUE(input_id);
  {
    // Click initially works.
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), input_id.value());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }
  ASSERT_TRUE(content::EvalJs(web_contents(), R"(
    let el = document.querySelector('#link');
    el.parentNode.removeChild(el);
  )")
                  .is_ok());
  {
    // After element removed, it doesn't work
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), input_id.value());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
  }
}

class ActorPageToolTimeoutBrowserTest : public ActorPageToolBrowserTest {
 public:
  ActorPageToolTimeoutBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor, {{"glic-actor-page-tool-timeout", "2s"}}},
         {features::kGlicActorIncrementalTyping,
          {{"glic-actor-long-text-paste-threshold", "1000000000"},
           {"glic-actor-incremental-typing-long-text-threshold",
            "1000000000"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Type so much text that a timeout occurs. Then, try again typing a single
// character, which should succeed.
IN_PROC_BROWSER_TEST_F(ActorPageToolTimeoutBrowserTest, Timeout) {
  const GURL url = embedded_test_server()->GetURL("/actor/cancel_typing.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Create a very long string (512KB) to type. This should take much longer
  // than the 2 sec timeout and ensure the test doesn't pass without the fix.
  std::string long_string(512 * 1024, 'a');

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  {
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), long_string,
                        /*follow_by_enter=*/false);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kToolTimeout);
  }

  // Now, try typing a short string. It should succeed.
  std::string short_string = "a";
  {
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), short_string,
                        /*follow_by_enter=*/false);

    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }
}

class ActorPageToolLongClickDelayBrowserTest
    : public ActorPageToolBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ActorPageToolLongClickDelayBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features_and_params;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features_and_params.push_back(
        {features::kGlicActor,
         // Delay holding the mouse down before mouse up.
         {{"glic-actor-click-delay", "2d"},
          {"glic-actor-page-tool-timeout", "2d"}}});

    if (GetParam()) {
      enabled_features_and_params.push_back(
          {features::kGlicActorMoveBeforeClick, {}});
    } else {
      disabled_features.push_back(features::kGlicActorMoveBeforeClick);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features_and_params,
                                                disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Click, then pause after mouse down. The click gets canceled, and the mouse
// button is lifted.
IN_PROC_BROWSER_TEST_P(ActorPageToolLongClickDelayBrowserTest, CancelClick) {
  const GURL url = embedded_test_server()->GetURL("/actor/cancel_click.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#clickable");
  ASSERT_TRUE(button_id);

  const bool move_before_click = GetParam();

  // First, try clicking and canceling. The mouse up should be sent right away.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());

    EXPECT_TRUE(content::ExecJs(main_frame(), R"(
      window.mousedownPromise = new Promise(resolve => {
        document.getElementById('clickable').addEventListener('mousedown',
                                                              resolve);
      });)",
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    ActResultFuture result_for_cancel;
    actor_task().Act(ToRequestList(action), result_for_cancel.GetCallback());

    // Wait for the mousedown event to be processed before pausing. This is
    // needed because the action starts asynchronously. Pausing the task will
    // immediately cancel the click before it can even start its delay.
    EXPECT_TRUE(content::ExecJs(main_frame(), "window.mousedownPromise"));

    // Pause the task, which should cancel the click and send the mouse
    // up right away.
    actor_task().Pause(/*from_actor=*/true);

    ExpectErrorResult(result_for_cancel, mojom::ActionResultCode::kTaskPaused);
    FlushChromeRenderFrameForTesting(*main_frame());

    const content::EvalJsResult eval_result =
        content::EvalJs(main_frame(), "event_log.join(',')");
    if (move_before_click) {
      EXPECT_EQ(
          "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
          "mouseup[BUTTON#clickable],click[BUTTON#clickable]",
          eval_result.ExtractString());
    } else {
      EXPECT_EQ(
          "mousedown[BUTTON#clickable],mouseup[BUTTON#clickable],"
          "click[BUTTON#clickable]",
          eval_result.ExtractString());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorPageToolLongClickDelayBrowserTest,
                         testing::Bool());

class ActorPageToolLongMouseMoveDelayBrowserTest
    : public ActorPageToolBrowserTest {
 public:
  ActorPageToolLongMouseMoveDelayBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor, {}},
                              {features::kGlicActorMoveBeforeClick,
                               // Delay after mouse move to target and before
                               // mouse down.
                               {{"glic-actor-move-before-click-delay", "2d"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Move mouse to target, then pause. The click gets canceled, but there's no
// mouse down or up.
IN_PROC_BROWSER_TEST_F(ActorPageToolLongMouseMoveDelayBrowserTest,
                       CancelDuringMouseMove) {
  const GURL url = embedded_test_server()->GetURL("/actor/cancel_click.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "#clickable");
  ASSERT_TRUE(button_id);

  // First, try clicking and canceling during the mouse move.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());

    EXPECT_TRUE(content::ExecJs(main_frame(), R"(
      window.mousemovePromise = new Promise(resolve => {
        document.getElementById('clickable').addEventListener('mousemove',
                                                              resolve);
      });)",
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    ActResultFuture result_for_cancel;
    actor_task().Act(ToRequestList(action), result_for_cancel.GetCallback());

    // Wait for the mousemove event to be processed before pausing.
    EXPECT_TRUE(content::ExecJs(main_frame(), "window.mousemovePromise"));

    // Pause the task, which should cancel the click.
    actor_task().Pause(/*from_actor=*/true);

    ExpectErrorResult(result_for_cancel, mojom::ActionResultCode::kTaskPaused);
    FlushChromeRenderFrameForTesting(*main_frame());

    // There should be a mouse move, but no other events.
    const content::EvalJsResult eval_result =
        content::EvalJs(main_frame(), "event_log.join(',')");
    EXPECT_EQ("mousemove[BUTTON#clickable]", eval_result.ExtractString());
  }
}

class ActorPageToolLongKeyDownDelayBrowserTest
    : public ActorPageToolBrowserTest {
 public:
  ActorPageToolLongKeyDownDelayBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor,
          // Delay holding down the keyboard key.
          {{"glic-actor-incremental-typing-key-down-duration", "2d"},
           {"glic-actor-page-tool-timeout", "2d"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Hold down a keyboard key, then pause. The key press gets canceled, and the
// key is lifted.
IN_PROC_BROWSER_TEST_F(ActorPageToolLongKeyDownDelayBrowserTest, CancelTyping) {
  const GURL url = embedded_test_server()->GetURL("/actor/cancel_typing.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  // First, try typing and canceling. The key up should be sent right away.
  {
    std::unique_ptr<ToolRequest> action =
        MakeTypeRequest(*main_frame(), input_id.value(), "a",
                        /*follow_by_enter=*/false);

    EXPECT_TRUE(content::ExecJs(main_frame(), R"(
      window.keydownPromise = new Promise(resolve => {
        document.getElementById('input').addEventListener('keydown',
                                                          resolve);
      });)",
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    ActResultFuture result_for_cancel;
    actor_task().Act(ToRequestList(action), result_for_cancel.GetCallback());

    // Wait for the keydown event to be processed before pausing.
    EXPECT_TRUE(content::ExecJs(main_frame(), "window.keydownPromise"));

    // Pause the task, which should cancel the typing and send the key up right
    // away.
    actor_task().Pause(/*from_actor=*/true);

    ExpectErrorResult(result_for_cancel, mojom::ActionResultCode::kTaskPaused);
    FlushChromeRenderFrameForTesting(*main_frame());

    content::EvalJsResult events_obj =
        content::EvalJs(main_frame(), "event_log.join(',')");
    EXPECT_EQ("keydown[INPUT#input],input[INPUT#input],keyup[INPUT#input]",
              events_obj.ExtractString());
  }
}

}  // namespace

}  // namespace actor
