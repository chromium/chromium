// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::ClickAction;
using ::testing::Property;

namespace actor {
namespace {

MATCHER_P(HasResultCode, expected_code, "") {
  return arg.action_result() == static_cast<int32_t>(expected_code);
}

class ActorFunctionalBrowserTest : public glic::test::InteractiveGlicTest {
 public:
  ActorFunctionalBrowserTest() {
    // TODO(crbug.com/453696965): Broken in multi-instance.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicMultiInstance});
  }
  ~ActorFunctionalBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    actor_keyed_service()->GetPolicyChecker().SetActOnWebForTesting(true);
    // TODO(crbug.com/461825458): Add support for kAttached window mode in test.
    RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  tabs::TabInterface* active_tab() {
    return browser()->tab_strip_model()->GetActiveTab();
  }

  actor::ActorKeyedService* actor_keyed_service() {
    return actor::ActorKeyedService::Get(browser()->profile());
  }

  // Helper that sets a future if an ActorTask with `task_id` enters a completed
  // state.
  base::CallbackListSubscription CreateTaskCompletetionSubscription(
      TaskId for_task_id,
      TestFuture<ActorTask::State>& future) {
    return actor_keyed_service()->AddTaskStateChangedCallback(
        base::BindLambdaForTesting([&future, for_task_id](
                                       TaskId task_id, ActorTask::State state) {
          if (task_id == for_task_id && ActorTask::IsCompletedState(state)) {
            future.SetValue(state);
          }
        }));
  }

  // Common helper to run EvalJs in the Glic frame.
  content::EvalJsResult EvalJsInGlic(const std::string& script) {
    return content::EvalJs(FindGlicGuestMainFrame(), script);
  }

  // Helper for JavaScript calls expected to return an integer.
  int EvalJsInGlicForInt(const std::string& script) {
    const auto js_result = EvalJsInGlic(script);
    EXPECT_THAT(js_result, content::EvalJsResult::IsOkAndHolds(
                               Property(&base::Value::is_int, true)))
        << "EvalJsInGlicForInt() failed or did not return an integer. Result: "
        << js_result;
    return js_result.ExtractInt();
  }

  // Helper for JavaScript calls that return a Base64 encoded string
  // representing a serialized protobuf of type `ProtoType`.
  // `proto_name` is used for error messages since RTTI is disabled.
  template <typename ProtoType>
  ProtoType EvalJsInGlicForBase64Proto(const std::string& script,
                                       const char* proto_name) {
    const auto js_result = EvalJsInGlic(script);
    EXPECT_THAT(js_result, content::EvalJsResult::IsOkAndHolds(
                               Property(&base::Value::is_string, true)))
        << "EvalJsInGlicForBase64Proto() for " << proto_name
        << " failed or did not return a string. Result: " << js_result;

    const std::string result_base64 = js_result.ExtractString();
    std::string decoded_result;
    CHECK(base::Base64Decode(result_base64, &decoded_result))
        << "Failed to Base64-decode the result for " << proto_name
        << " from JavaScript.";

    ProtoType proto_result;
    CHECK(proto_result.ParseFromString(decoded_result))
        << "Failed to parse " << proto_name
        << " proto from the decoded result.";
    return proto_result;
  }

  // Helper to call the CreateTask TS API.
  // Returns the TaskId of the newly created ActorTask.
  TaskId CreateTask(webui::mojom::TaskOptionsPtr options = nullptr) {
    std::string title;
    if (options && options->title) {
      title = content::JsReplace("{title: $1}", *options->title);
    }

    std::string script =
        base::StrCat({"window.client.browser.createTask(", title, ");"});

    return actor::TaskId(EvalJsInGlicForInt(script));
  }

  // Helper to call the PerformActions TS API.
  // Takes an `Actions` proto and returns the resulting `ActionsResult` proto.
  [[nodiscard]] optimization_guide::proto::ActionsResult PerformActions(
      const optimization_guide::proto::Actions& actions) {
    std::string serialized_actions;
    CHECK(actions.SerializeToString(&serialized_actions));
    const std::string proto_base64 = base::Base64Encode(serialized_actions);

    const std::string script = R"(
      (async (protoAsBase64) => {
        // Manually decode the base64 string into a Uint8Array.
        const binaryString = atob(protoAsBase64);
        const len = binaryString.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }

        // Call performActions with the ArrayBuffer.
        const resultBuffer =
            await window.client.browser.performActions(bytes.buffer);

        // Manually encode the resulting ArrayBuffer back to a base64 string
        // to return to C++.
        const resultBytes = new Uint8Array(resultBuffer);
        let resultBinaryString = '';
        for (let i = 0; i < resultBytes.length; i++) {
            resultBinaryString += String.fromCharCode(resultBytes[i]);
        }
        return btoa(resultBinaryString);
      })($1)
    )";

    return EvalJsInGlicForBase64Proto<optimization_guide::proto::ActionsResult>(
        content::JsReplace(script, proto_base64),
        "optimization_guide::proto::ActionsResult");
  }

  // Helper to call the StopActorTask TS API.
  // Note: Inactive tasks are cleared right after entering a "Completed" state,
  // so you need to listen for state changes using a subscription before calling
  // this method if you want to verify the task stopped correctly.
  void StopActorTask(TaskId task_id,
                     glic::mojom::ActorTaskStopReason stop_reason) {
    std::string script = R"(
      (async (taskId, stopReason) => {
        await window.client.browser.stopActorTask(taskId, stopReason);
      })($1, $2)
    )";
    const auto stop_task_js_result = EvalJsInGlic(content::JsReplace(
        script, task_id.value(), static_cast<int>(stop_reason)));
    EXPECT_THAT(stop_task_js_result, content::EvalJsResult::IsOk())
        << "stopActorTask() failed.";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       CreateTask_Navigate_StopTask) {
  TaskId task_id = CreateTask();
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletetionSubscription(task_id, task_completion_state);

  // Construct the Actions proto.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  optimization_guide::proto::Actions action =
      MakeNavigate(active_tab()->GetHandle(), target_url.spec());
  action.set_task_id(task_id.value());

  EXPECT_THAT(PerformActions(action),
              HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, CreateTask_Click_StopTask) {
  // Set up the initial page with a link to the target page.
  const GURL initial_url = embedded_test_server()->GetURL("/actor/link.html");
  const GURL target_url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));
  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", target_url)));

  TaskId task_id = CreateTask();
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletetionSubscription(task_id, task_completion_state);

  // Click link to navigate to target page.
  std::optional<int> link_node_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#link");
  optimization_guide::proto::Actions actions =
      MakeClick(*web_contents()->GetPrimaryMainFrame(), link_node_id.value(),
                ClickAction::LEFT, ClickAction::SINGLE);
  actions.set_task_id(task_id.value());

  EXPECT_THAT(PerformActions(actions),
              HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

}  // namespace
}  // namespace actor
