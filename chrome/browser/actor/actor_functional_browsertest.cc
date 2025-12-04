// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/types/expected_macros.h"
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

namespace mojo {
template <>
struct TypeConverter<base::Value, glic::mojom::GetTabContextOptions> {
  static base::Value Convert(const glic::mojom::GetTabContextOptions in) {
    base::Value raw_out(base::Value::Type::DICT);
    base::Value::Dict& out = raw_out.GetDict();
    out.Set("includeInnerText", in.include_inner_text);
    out.Set("innerTextBytesLimit", static_cast<int>(in.inner_text_bytes_limit));
    out.Set("includeViewportScreenshot", in.include_viewport_screenshot);
    out.Set("includeAnnotatedPageContent", in.include_annotated_page_content);
    out.Set("maxMetaTags", static_cast<int>(in.max_meta_tags));
    out.Set("includePdf", in.include_pdf);
    out.Set("pdfSizeLimit", static_cast<int>(in.pdf_size_limit));
    out.Set("annotatedPageContentMode",
            static_cast<int>(in.annotated_page_content_mode));
    return raw_out;
  }
};
}  // namespace mojo

namespace actor {
namespace {

MATCHER_P(HasResultCode, expected_code, "") {
  return arg.action_result() == static_cast<int32_t>(expected_code);
}

// Matches a base::expected<T, std::string> which has an error string
// that contains `expected_substring`.
MATCHER_P(ErrorHasSubstr, expected_substring, "") {
  return testing::Matches(
      base::test::ErrorIs(testing::HasSubstr(expected_substring)))(arg);
}

// Helper to convert a content::EvalJsResult to a
// base::expected<base::Value, std::string>.
base::expected<base::Value, std::string> ToExpected(
    content::EvalJsResult result) {
  if (!result.is_ok()) {
    return base::unexpected(result.ExtractError());
  }
  return base::ok(std::move(result).TakeValue());
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
  base::CallbackListSubscription CreateTaskCompletionSubscription(
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
  base::expected<base::Value, std::string> EvalJsInGlic(
      const std::string_view script) {
    return ToExpected(content::EvalJs(FindGlicGuestMainFrame(), script));
  }

  // Helper for JavaScript calls expected to return an integer value.
  base::expected<int, std::string> EvalJsInGlicForInt(
      const std::string_view script) {
    ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
    if (std::optional<int> result = js_result.GetIfInt()) {
      return *result;
    }
    return base::unexpected("Expected an integer value from JavaScript.");
  }

  // Helper for JavaScript calls that return a Base64 encoded string
  // representing a serialized protobuf of type `ProtoType`.
  // `proto_name` is used for error messages since RTTI is disabled.
  template <typename ProtoType>
  base::expected<ProtoType, std::string> EvalJsInGlicForBase64Proto(
      std::string_view script,
      std::string_view proto_name) {
    ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
    const std::string* result_base64 = js_result.GetIfString();
    if (!result_base64) {
      return base::unexpected("Expected a string value from JavaScript.");
    }
    std::string decoded_result;
    ProtoType proto_result;
    if (!base::Base64Decode(*result_base64, &decoded_result)) {
      return base::unexpected(
          base::StrCat({"Failed to Base64-decode the result for ", proto_name,
                        " from JavaScript."}));
    }

    if (!proto_result.ParseFromString(decoded_result)) {
      return base::unexpected(base::StrCat(
          {"Failed to parse ", proto_name, " proto from the decoded result."}));
    }
    return base::ok(proto_result);
  }

  // Helper to call the CreateTask TS API.
  // Returns the TaskId of the newly created ActorTask.
  base::expected<TaskId, std::string> CreateTask(
      webui::mojom::TaskOptionsPtr options = nullptr) {
    std::string title;
    if (options && options->title) {
      title = content::JsReplace("{title: $1}", *options->title);
    }

    std::string script =
        base::StrCat({"window.client.browser.createTask(", title, ");"});
    ASSIGN_OR_RETURN(int task_id_int, EvalJsInGlicForInt(script));
    return base::ok(actor::TaskId(task_id_int));
  }

  // Helper to call the PerformActions TS API.
  // Takes an `Actions` proto and returns the resulting `ActionsResult` proto.
  [[nodiscard]] optimization_guide::proto::ActionsResult PerformActions(
      const optimization_guide::proto::Actions& actions) {
    // TODO(crbug.com/465206246): Revise the PerformActions helper method to
    // support async requests.
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

    const std::string full_script = content::JsReplace(script, proto_base64);
    base::expected<optimization_guide::proto::ActionsResult, std::string>
        result_expected = EvalJsInGlicForBase64Proto<
            optimization_guide::proto::ActionsResult>(
            full_script, "optimization_guide::proto::ActionsResult");
    CHECK(result_expected.has_value()) << result_expected.error();
    return result_expected.value();
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
    // Store the result of content::JsReplace in a std::string to make ownership
    // explicit.
    const std::string full_script = content::JsReplace(
        script, task_id.value(), static_cast<int>(stop_reason));
    EXPECT_OK(EvalJsInGlic(full_script));
  }

  // Helper to call the PauseActorTask TS API.
  void PauseActorTask(TaskId task_id,
                      glic::mojom::ActorTaskPauseReason pause_reason =
                          glic::mojom::ActorTaskPauseReason::kPausedByModel,
                      tabs::TabHandle tab_handle = tabs::TabHandle::Null()) {
    base::expected<base::Value, std::string> pause_task_js_result = [&]() {
      if (tab_handle == tabs::TabHandle::Null()) {
        std::string script = "window.client.browser.pauseActorTask($1, $2);";
        return EvalJsInGlic(content::JsReplace(script, task_id.value(),
                                               static_cast<int>(pause_reason)));
      } else {
        std::string script =
            "window.client.browser.pauseActorTask($1, $2, $3);";
        return EvalJsInGlic(content::JsReplace(
            script, task_id.value(), static_cast<int>(pause_reason),
            base::NumberToString(tab_handle.raw_value())));
      }
    }();

    EXPECT_TRUE(pause_task_js_result.has_value())
        << "pauseActorTask() failed: " << pause_task_js_result.error();
  }

  // Helper to call the ResumeActorTask TS API.
  // Returns the ActionResultCode of the resumeActorTask call.
  base::expected<mojom::ActionResultCode, std::string> ResumeActorTask(
      TaskId task_id,
      base::Value context_options) {
    ASSIGN_OR_RETURN(
        std::string context_options_json,
        base::WriteJson(context_options.GetDict()), [&]() {
          return std::string("Failed to serialize context options to JSON.");
        });

    std::string script = base::StringPrintf(
        "(async () => {"
        "  const result = await window.client.browser.resumeActorTask(%d, %s);"
        "  return result.actionResult;"
        "})()",
        task_id.value(), context_options_json.c_str());
    ASSIGN_OR_RETURN(int action_result_int, EvalJsInGlicForInt(script));
    return base::ok(static_cast<mojom::ActionResultCode>(action_result_int));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/465188408): Move all test cases to dedicated files grouped by
// the functionality being tested.
IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       CreateTask_Navigate_StopTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

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

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

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

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PauseAndResumeCreatedTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  // Wait for the task to pause.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return actor_keyed_service()->GetTask(task_id)->GetState() ==
           ActorTask::State::kPausedByUser;
  })) << "Timed out waiting for task "
      << task_id << " to pause.";

  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  optimization_guide::proto::Actions action =
      MakeNavigate(active_tab()->GetHandle(), target_url.spec());
  action.set_task_id(task_id.value());

  // Performing an action on a paused task should fail.
  EXPECT_THAT(PerformActions(action),
              HasResultCode(mojom::ActionResultCode::kTaskPaused));
  EXPECT_NE(target_url, web_contents()->GetURL());

  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>())
          .value(),
      testing::Eq(mojom::ActionResultCode::kOk));
  EXPECT_EQ(ActorTask::State::kReflecting,
            actor_keyed_service()->GetTask(task_id)->GetState());

  // Performing the action again should succeed.
  EXPECT_THAT(PerformActions(action),
              HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PauseAndResumeInvalidTask) {
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  // Pausing an invalid task should be a no-op.
  PauseActorTask(invalid_task_id,
                 glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  EXPECT_THAT(
      ResumeActorTask(invalid_task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ErrorHasSubstr("resumeActorTask failed: No such task"));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PauseAndResumeInactiveTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";

  // Pausing an inactive task should be a no-op.
  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  // Resuming a completed task should fail as it doesn't exist anymore.
  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ErrorHasSubstr("resumeActorTask failed: No such task"));
}

}  // namespace
}  // namespace actor
