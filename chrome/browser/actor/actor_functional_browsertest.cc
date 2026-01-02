// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

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

using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::TabObservation;
using ::testing::Property;

// Helper to mock the result returned on a TabObservation built using
// actor::BuildActionsResultWithObservations. While live, use the provided
// function to set TabObservationResults. Unset on destruction.
class ScopedMockTabObservationResult {
 public:
  explicit ScopedMockTabObservationResult(
      base::RepeatingCallback<TabObservation::TabObservationResult()>
          callback) {
    SetTabObservationResultOverrideForTesting(callback);
  }
  ~ScopedMockTabObservationResult() {
    SetTabObservationResultOverrideForTesting(
        base::RepeatingCallback<TabObservation::TabObservationResult()>());
  }
};

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

Actions MakeWaitForTaskId(base::TimeDelta duration, int task_id) {
  Actions action = MakeWait(duration);
  action.set_task_id(task_id);
  return action;
}

// Helper class that utilizes content::DOMMessageQueue to capture the result of
// an asynchronous PerformActions call. It listens for messages sent via
// domAutomationController and filters by request ID to ensure the correct
// result is returned.
class AsyncActionWaiter {
 public:
  AsyncActionWaiter(content::RenderFrameHost* rfh, std::string request_id)
      : queue_(rfh), request_id_(std::move(request_id)) {}

  base::expected<ActionsResult, std::string> Wait() {
    while (true) {
      std::string json_message;
      if (!queue_.WaitForMessage(&json_message)) {
        return base::unexpected("Failed to wait for message from JS.");
      }

      auto json_value = base::JSONReader::ReadAndReturnValueWithError(
          json_message, base::JSON_PARSE_RFC);
      if (!json_value.has_value()) {
        return base::unexpected("Failed to parse JSON result from JS: " +
                                json_value.error().message);
      }

      const base::Value::Dict* dict = json_value->GetIfDict();
      if (!dict) {
        return base::unexpected("Expected a JSON object from JS.");
      }

      const std::string* id = dict->FindString("requestId");
      if (!id) {
        return base::unexpected(
            "Expected a string value for `requestId` key in JSON object from "
            "JS");
      }

      if (*id != request_id_) {
        // Message not for us
        continue;
      }

      const std::string* result_base64 = dict->FindString("result");
      if (!result_base64) {
        return base::unexpected("JSON result missing 'result' field.");
      }

      return ParseBase64Proto<ActionsResult>(*result_base64);
    }
  }

 private:
  content::DOMMessageQueue queue_;
  std::string request_id_;
};

class ActorFunctionalBrowserTest : public glic::test::InteractiveGlicTest {
 public:
  ActorFunctionalBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicMultiInstance,
                              actor::kActorBindCreatedTabToTask},
        /*disabled_features=*/{});
  }
  ~ActorFunctionalBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    actor_keyed_service()->GetPolicyChecker().set_act_on_web_for_testing(true);
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

  base::expected<std::string, std::string> EvalJsInGlicForString(
      const std::string_view script) {
    ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
    if (std::string* result = js_result.GetIfString()) {
      return base::ok(*result);
    }
    return base::unexpected("Expected a string value from JavaScript.");
  }

  // Helper for JavaScript calls that return a Base64 encoded string
  // representing a serialized protobuf of type `ProtoType`.
  template <typename ProtoType>
  base::expected<ProtoType, std::string> EvalJsInGlicForBase64Proto(
      std::string_view script) {
    ASSIGN_OR_RETURN(base::Value js_result, EvalJsInGlic(script));
    const std::string* result_base64 = js_result.GetIfString();
    if (!result_base64) {
      return base::unexpected("Expected a string value from JavaScript.");
    }
    return ParseBase64Proto<ProtoType>(*result_base64);
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

  // Helper to call the PerformActions TS API synchronously.
  // Takes an `Actions` proto and returns the resulting `ActionsResult` proto.
  // Note: This blocks until all Actions are completed by wrapping
  // PerformActionsAsync.
  [[nodiscard]] base::expected<ActionsResult, std::string> PerformActions(
      const Actions& actions) {
    return PerformActionsAsync(actions)->Wait();
  }

  // Helper to run PerformActions asynchronously.
  // Returns an AsyncActionWaiter that can be used to wait for the result.
  [[nodiscard]] std::unique_ptr<AsyncActionWaiter> PerformActionsAsync(
      const Actions& actions) {
    // TODO(crbug.com/471254787): Revise PerformActionsAsync to handle async JS
    // calls in a blocking manner in C++.
    std::string serialized_actions;
    CHECK(actions.SerializeToString(&serialized_actions));
    const std::string proto_base64 = base::Base64Encode(serialized_actions);

    static int counter = 0;
    std::string request_id = base::NumberToString(++counter);
    auto waiter = std::make_unique<AsyncActionWaiter>(FindGlicGuestMainFrame(),
                                                      request_id);
    // Script to call PerformActions() and send the result via
    // domAutomationController to be received by the AsyncActionWaiter.
    const std::string script = content::JsReplace(
        R"(
      (async () => {
        const resultBuffer =
            await window.client.browser.performActions(
                Uint8Array.fromBase64($1).buffer);
        window.domAutomationController.send({
            requestId: $2,
            result: new Uint8Array(resultBuffer).toBase64()
        });
      })();
    )",
        proto_base64, request_id);

    content::ExecuteScriptAsync(FindGlicGuestMainFrame(), script);

    return waiter;
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

  // Helper to call the CreateActorTab TS API.
  // Returns the TabId of the newly created tab, or base::unexpected on failure.
  base::expected<tabs::TabHandle, std::string> CreateActorTab(
      TaskId task_id,
      std::optional<bool> open_in_background,
      std::optional<std::string> initiator_tab_id,
      std::optional<std::string> initiator_window_id) {
    static constexpr std::string_view kCreateActorTabScript = R"(
      (async (taskId, openInBackground, initiatorTabId, initiatorWindowId) => {
        const options = {};
        if (openInBackground !== null) {
          options.openInBackground = openInBackground;
        }
        if (initiatorTabId !== null) {
          options.initiatorTabId = initiatorTabId;
        }
        if (initiatorWindowId !== null) {
          options.initiatorWindowId = initiatorWindowId;
        }
        const tabData = await window.client.browser.createActorTab(
            taskId, options);
        // "NO_TAB_ID" triggers the parsing error on C++ side.
        return tabData ? tabData.tabId : "NO_TAB_ID";
      })($1, $2, $3, $4)
    )";
    base::expected<std::string, std::string> result =
        EvalJsInGlicForString(content::JsReplace(
            kCreateActorTabScript, task_id.value(),
            open_in_background ? base::Value(*open_in_background)
                               : base::Value(),
            initiator_tab_id ? base::Value(*initiator_tab_id) : base::Value(),
            initiator_window_id ? base::Value(*initiator_window_id)
                                : base::Value()));
    if (!result.has_value()) {
      return base::unexpected(result.error());
    }
    int tab_id;
    if (!base::StringToInt(result.value(), &tab_id)) {
      return base::unexpected(base::StringPrintf(
          "Failed to parse tab ID %s from TS API.", result.value().c_str()));
    }
    return tabs::TabHandle(tab_id);
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
  Actions action = MakeNavigate(active_tab()->GetHandle(), target_url.spec());
  action.set_task_id(task_id.value());

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
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
  Actions action =
      MakeClick(*web_contents()->GetPrimaryMainFrame(), link_node_id.value(),
                ClickAction::LEFT, ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
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
  Actions action = MakeNavigate(active_tab()->GetHandle(), target_url.spec());
  action.set_task_id(task_id.value());

  // Performing an action on a paused task should fail.
  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kTaskPaused)));
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
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
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

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       PerformConcurrentAsyncWaitActions) {
  // Manually create tasks via ActorKeyedService.
  TaskId task_id_1 = actor_keyed_service()->CreateTask();
  TaskId task_id_2 = actor_keyed_service()->CreateTask();

  // Perform two WaitActions where the first resolves after the second
  Actions action_1 =
      MakeWaitForTaskId(base::Milliseconds(20), task_id_1.value());
  Actions action_2 =
      MakeWaitForTaskId(base::Milliseconds(10), task_id_2.value());

  std::unique_ptr<AsyncActionWaiter> waiter_1 = PerformActionsAsync(action_1);
  std::unique_ptr<AsyncActionWaiter> waiter_2 = PerformActionsAsync(action_2);

  // We should still be able to wait for result_2 after result_1 despite
  // action_2 resolving first.
  ASSERT_OK_AND_ASSIGN(ActionsResult result_1, waiter_1->Wait());
  ASSERT_OK_AND_ASSIGN(ActionsResult result_2, waiter_2->Wait());

  EXPECT_THAT(result_1, HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_2, HasResultCode(mojom::ActionResultCode::kOk));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       PerformActionsOnCrashedTabReloadsTab) {
  const GURL& initial_url = web_contents()->GetLastCommittedURL();
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Crash the tab.
  content::CrashTab(web_contents());

  // Perform a click action on the crashed tab.
  Actions action = MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                             ::optimization_guide::proto::ClickAction::LEFT,
                             ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  content::TestNavigationManager reload_observer(web_contents(), initial_url);
  EXPECT_THAT(
      PerformActions(action),
      ValueIs(HasResultCode(mojom::ActionResultCode::kRendererCrashed)));
  EXPECT_TRUE(reload_observer.WaitForNavigationFinished());
  EXPECT_FALSE(web_contents()->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       RetryFailedContextFetchAfterPerformActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  ::optimization_guide::proto::Actions action =
      MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                ::optimization_guide::proto::ClickAction::LEFT,
                ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  // Mock the context fetch so that the first time the TabObservationResult is a
  // failure. This should result in a retry which then succeeds.
  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting([&]() {
    ++num_calls;
    if (num_calls == 1) {
      return TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE;
    }

    return TabObservation::TAB_OBSERVATION_OK;
  }));

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(num_calls, 2);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       FailedContextFetchOnlyRetriesOnce) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  ::optimization_guide::proto::Actions action =
      MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                ::optimization_guide::proto::ClickAction::LEFT,
                ::optimization_guide::proto::ClickAction::SINGLE);
  action.set_task_id(task_id.value());

  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting([&]() {
    ++num_calls;
    return TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE;
  }));

  optimization_guide::proto::ActionsResult result =
      PerformActions(action).value();
  EXPECT_THAT(result, HasResultCode(mojom::ActionResultCode::kOk));
  ASSERT_EQ(result.tabs_size(), 1);
  ASSERT_TRUE(result.tabs().at(0).has_result());
  EXPECT_EQ(result.tabs().at(0).result(),
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);

  EXPECT_EQ(num_calls, 2);
}

class ActorFunctionalBrowserTestCreateActorTab
    : public ActorFunctionalBrowserTest,
      public ::testing::WithParamInterface<GURL> {
 public:
  ActorFunctionalBrowserTestCreateActorTab() = default;
  ~ActorFunctionalBrowserTestCreateActorTab() override = default;

  GURL GetInitiatorTabUrl() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ActorFunctionalBrowserTestCreateActorTab,
                       CreateActorTab) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1u);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  // Create a new tab for the task.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(task_id.value(), /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));
  ASSERT_TRUE(new_tab_handler.has_value()) << new_tab_handler.error();

  // Verify it is bound to the task.
  EXPECT_TRUE(actor_keyed_service()
                  ->GetTask(task_id.value())
                  ->GetTabs()
                  .contains(new_tab_handler.value()));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorFunctionalBrowserTestCreateActorTab,
    ::testing::Values(GURL(chrome::kChromeUINewTabURL),
                      GURL(url::kAboutBlankURL)));

}  // namespace
}  // namespace actor
