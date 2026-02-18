// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_functional_browsertest.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
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
    base::DictValue& out = raw_out.GetDict();
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
using ::glic::test::ErrorHasSubstr;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::TabObservation;
using ::page_content_annotations::FetchPageContextResult;
using ::testing::Property;

// Helper class to observe journal entries and wait for a specific condition.
class JournalObserver : public actor::AggregatedJournal::Observer {
 public:
  using Predicate =
      base::RepeatingCallback<bool(const actor::mojom::JournalEntry&)>;

  explicit JournalObserver(actor::AggregatedJournal* journal)
      : journal_(journal) {
    journal_->AddObserver(this);
  }

  ~JournalObserver() override { journal_->RemoveObserver(this); }

  void WillAddJournalEntry(
      const actor::AggregatedJournal::Entry& entry) override {
    if (wait_predicate_ && wait_predicate_.Run(*entry.data)) {
      if (run_loop_) {
        run_loop_->Quit();
      }
    }
  }

  // Waits until a journal entry matching the predicate is observed.
  // NOTE: Only entries added after this method is called will be considered.
  void WaitUntil(Predicate predicate) {
    wait_predicate_ = std::move(predicate);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  raw_ptr<actor::AggregatedJournal> journal_;
  Predicate wait_predicate_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

bool JournalEntryHasError(const actor::mojom::JournalEntry& entry,
                          const std::string& error_message) {
  for (const auto& detail : entry.details) {
    if (detail->key == "error" && detail->value == error_message) {
      return true;
    }
  }
  return false;
}

// Helper to mock the result returned on a TabObservation built using
// actor::BuildActionsResultWithObservations. While live, use the provided
// function to set TabObservationResults. Unset on destruction.
class ScopedMockTabObservationResult {
 public:
  explicit ScopedMockTabObservationResult(
      base::RepeatingCallback<void(TabObservation*,
                                   const FetchPageContextResult&)> callback) {
    SetTabObservationResultOverrideForTesting(callback);
  }
  ~ScopedMockTabObservationResult() {
    SetTabObservationResultOverrideForTesting(
        base::RepeatingCallback<void(TabObservation*,
                                     const FetchPageContextResult&)>());
  }
};

MATCHER_P(HasResultCode, expected_code, "") {
  return arg.action_result() == static_cast<int32_t>(expected_code);
}

Actions MakeWaitForTaskId(std::optional<base::TimeDelta> duration,
                          std::optional<tabs::TabHandle> observe_tab_handle,
                          TaskId task_id) {
  Actions action = MakeWait(duration, observe_tab_handle);
  action.set_task_id(task_id.value());
  return action;
}

Actions MakeNavigateForTaskId(tabs::TabHandle tab_handle,
                              std::string_view target_url_spec,
                              TaskId task_id) {
  Actions action = MakeNavigate(tab_handle, target_url_spec);
  action.set_task_id(task_id.value());
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

      const base::DictValue* dict = json_value->GetIfDict();
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

class ActorFunctionalBrowserTest
    : public glic::test::GlicFunctionalBrowserTestBase {
 public:
  static constexpr base::TimeDelta kShortWaitTime = base::Milliseconds(10);
  static constexpr base::TimeDelta kLongWaitTime = base::Minutes(2);

  ActorFunctionalBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicMultiInstance, {}},
                              {actor::kActorBindCreatedTabToTask, {}},
                              {features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "true"}}}},
        /*disabled_features=*/{});
  }
  ~ActorFunctionalBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    glic::test::GlicFunctionalBrowserTestBase::SetUpOnMainThread();
    // TODO(crbug.com/461825458): Add support for kAttached window mode in test.
    RunTestSequence(OpenGlic());
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

  // Returns the state of the relevant ActorTask.
  ActorTask::State GetActorTaskState(TaskId task_id) {
    ActorTask* task = actor_keyed_service()->GetTask(task_id);
    CHECK_NE(task, nullptr) << "ActorTask " << task_id << " not found.";
    return task->GetState();
  }

  base::expected<glic::mojom::CancelActionsResult, std::string> CancelActions(
      TaskId task_id) {
    std::string script = "window.client.browser.cancelActions($1)";
    ASSIGN_OR_RETURN(int result_int, EvalJsInGlicForInt(content::JsReplace(
                                         script, task_id.value())));
    return base::ok(static_cast<glic::mojom::CancelActionsResult>(result_int));
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
  // Note: `tab_handle` needs to be specified if you intend to resume the task
  // in the future without performing any tab-scoped actions beforehand.
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

  // Helper to call the InterruptActorTask TS API.
  void InterruptActorTask(TaskId task_id) {
    std::string script = "window.client.browser.interruptActorTask($1);";
    EXPECT_OK(EvalJsInGlic(content::JsReplace(script, task_id.value())));
  }

  // Helper to call the UninterruptActorTask TS API.
  void UninterruptActorTask(TaskId task_id) {
    std::string script = "window.client.browser.uninterruptActorTask($1);";
    EXPECT_OK(EvalJsInGlic(content::JsReplace(script, task_id.value())));
  }

  // Waits until the task reaches the `expected_state`.
  void WaitForTaskState(TaskId task_id, ActorTask::State expected_state) {
    if (actor_keyed_service()->GetTask(task_id)->GetState() == expected_state) {
      return;
    }
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        actor_keyed_service()->AddTaskStateChangedCallback(
            base::BindLambdaForTesting(
                [&](TaskId task_id_param, ActorTask::State state) {
                  if (task_id_param == task_id && state == expected_state) {
                    run_loop.Quit();
                  }
                }));
    run_loop.Run();
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
  Actions action = MakeNavigateForTaskId(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

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
  WaitForTaskState(task_id, ActorTask::State::kPausedByUser);

  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = MakeNavigateForTaskId(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  // Performing an action on a paused task should fail.
  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kTaskPaused)));
  EXPECT_NE(target_url, web_contents()->GetURL());

  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ValueIs(mojom::ActionResultCode::kOk));
  EXPECT_EQ(ActorTask::State::kReflecting, GetActorTaskState(task_id));

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

  JournalObserver observer(&actor_keyed_service()->GetJournal());
  // Pausing an invalid task should be a no-op and log an error.
  PauseActorTask(invalid_task_id,
                 glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  observer.WaitUntil(
      base::BindRepeating([](const actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

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

  JournalObserver observer(&actor_keyed_service()->GetJournal());
  // Pausing an inactive task should be a no-op and log an error.
  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());
  observer.WaitUntil(
      base::BindRepeating([](const actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to pause task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  // Resuming a completed task should fail as it doesn't exist anymore.
  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ErrorHasSubstr("resumeActorTask failed: No such task"));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       PerformConcurrentAsyncWaitActions) {
  // Manually create tasks via ActorKeyedService.
  TaskId task_id_1 =
      actor_keyed_service()->CreateTask(NoEnterprisePolicyChecker());
  TaskId task_id_2 =
      actor_keyed_service()->CreateTask(NoEnterprisePolicyChecker());

  // Create tabs for each task using CreateActorTab API to ensure a
  // TabObservation is included in its result.
  ASSERT_OK_AND_ASSIGN(
      tabs::TabHandle tab_1,
      CreateActorTab(task_id_1, /*open_in_background=*/false,
                     base::ToString(active_tab()->GetHandle().raw_value()),
                     base::ToString(browser()->session_id().id())));
  ASSERT_OK_AND_ASSIGN(
      tabs::TabHandle tab_2,
      CreateActorTab(task_id_2, /*open_in_background=*/false,
                     base::ToString(active_tab()->GetHandle().raw_value()),
                     base::ToString(browser()->session_id().id())));

  // Perform two WaitActions where the first resolves after the second
  Actions action_1 = MakeWaitForTaskId(kShortWaitTime * 2, tab_1, task_id_1);
  Actions action_2 = MakeWaitForTaskId(kShortWaitTime, tab_2, task_id_2);

  std::unique_ptr<AsyncActionWaiter> waiter_1 = PerformActionsAsync(action_1);
  std::unique_ptr<AsyncActionWaiter> waiter_2 = PerformActionsAsync(action_2);

  // We should still be able to wait for result_2 after result_1 despite
  // action_2 resolving first.
  ASSERT_OK_AND_ASSIGN(ActionsResult result_1, waiter_1->Wait());
  ASSERT_OK_AND_ASSIGN(ActionsResult result_2, waiter_2->Wait());

  // Verify a tab observation was included in the results.
  EXPECT_THAT(result_1, HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_1.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_1.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);

  EXPECT_THAT(result_2, HasResultCode(mojom::ActionResultCode::kOk));
  EXPECT_THAT(result_2.tabs(), testing::SizeIs(1));
  EXPECT_THAT(result_2.tabs().at(0).result(),
              TabObservation::TAB_OBSERVATION_OK);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, PauseActiveTask) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Use a long wait to ensure we can pause before it completes.
  optimization_guide::proto::Actions wait_action =
      MakeWaitForTaskId(kLongWaitTime, active_tab()->GetHandle(), task_id);

  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);
  PauseActorTask(task_id, glic::mojom::ActorTaskPauseReason::kPausedByUser,
                 active_tab()->GetHandle());

  // Verify the WaitAction was ended and the task was paused.
  EXPECT_THAT(action_waiter->Wait(),
              ValueIs(HasResultCode(mojom::ActionResultCode::kTaskPaused)));
  WaitForTaskState(task_id, ActorTask::State::kPausedByUser);

  EXPECT_THAT(
      ResumeActorTask(task_id,
                      glic::mojom::GetTabContextOptions().To<base::Value>()),
      ValueIs(mojom::ActionResultCode::kOk));
  EXPECT_EQ(ActorTask::State::kReflecting, GetActorTaskState(task_id));

  // Verify new Actions can be performed after the task is resumed.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  optimization_guide::proto::Actions nav_action = MakeNavigateForTaskId(
      active_tab()->GetHandle(), target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(nav_action),
              base::test::ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       StopActiveTaskWithModelError) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  optimization_guide::proto::Actions wait_action =
      MakeWaitForTaskId(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before stopping.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Verify the action is ended with the appropriate code after task is stopped.
  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kModelError);
  EXPECT_THAT(action_waiter->Wait(),
              ValueIs(HasResultCode(mojom::ActionResultCode::kTaskWentAway)));

  EXPECT_EQ(ActorTask::State::kFailed, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFailed state.";
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, CloseTabWhileActing) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  optimization_guide::proto::Actions wait_action =
      MakeWaitForTaskId(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before closing the tab.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Add a new background tab to prevent the browser from closing.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the active web contents.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents()),
      TabCloseTypes::CLOSE_NONE);

  // After an acting tab is closed, the task should be cancelled and the
  // corresponding action have a result code of kTaskWentAway.
  // NOTE: We cannot use `action_waiter->Wait()` to check the result code
  // because the test client is destroyed when all task tabs are closed.
  EXPECT_EQ(ActorTask::State::kCancelled, task_completion_state.Get());
  histogram_tester.ExpectUniqueSample("Actor.ExecutionEngine.Action.ResultCode",
                                      mojom::ActionResultCode::kTaskWentAway,
                                      1);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       InterruptAndUninterruptInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  // Interrupting an invalid task should be a no-op and log an error.
  InterruptActorTask(invalid_task_id);
  observer.WaitUntil(
      base::BindRepeating([](const actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to interrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));

  // Uninterrupting an invalid task should be a no-op and log an error.
  UninterruptActorTask(invalid_task_id);
  observer.WaitUntil(
      base::BindRepeating([](const actor::mojom::JournalEntry& entry) {
        return entry.event == "Failed to uninterrupt task" &&
               JournalEntryHasError(entry, "No such task");
      }));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       InterruptAndUninterruptTaskWithCompletedActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  Actions action = MakeNavigateForTaskId(active_tab()->GetHandle(),
                                         target_url.spec(), task_id);

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  InterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);

  // Ensure uninterrupting a task with no pending actions sets the state
  // to kReflecting
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kReflecting);

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       InterruptAndUninterruptActiveTaskAndPerformActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  // Use a long wait to ensure we can interrupt before it completes.
  optimization_guide::proto::Actions wait_action =
      MakeWaitForTaskId(kLongWaitTime, active_tab()->GetHandle(), task_id);
  std::unique_ptr<AsyncActionWaiter> action_waiter =
      PerformActionsAsync(wait_action);

  // Wait for the task to start acting before interrupting.
  WaitForTaskState(task_id, ActorTask::State::kActing);

  InterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kWaitingOnUser);

  // Ensure uninterrupting a task with previously pending actions sets the state
  // to kActing
  UninterruptActorTask(task_id);
  WaitForTaskState(task_id, ActorTask::State::kActing);

  // Since the ongoing long wait action must be completed before sending another
  // async action, we need to use the CancelActions API to cancel all the
  // ongoing actions on the task.
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_THAT(
      action_waiter->Wait(),
      ValueIs(HasResultCode(mojom::ActionResultCode::kActionsCancelled)));

  // Ensure the task can still perform actions after being uninterrupted.
  const GURL target_url =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  optimization_guide::proto::Actions nav_action = MakeNavigateForTaskId(
      active_tab()->GetHandle(), target_url.spec(), task_id);
  EXPECT_THAT(PerformActions(nav_action),
              base::test::ValueIs(HasResultCode(mojom::ActionResultCode::kOk)));
  EXPECT_EQ(target_url, web_contents()->GetURL());

  StopActorTask(task_id, glic::mojom::ActorTaskStopReason::kTaskComplete);
  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
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
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        if (num_calls == 1) {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
        } else {
          observation->set_result(TabObservation::TAB_OBSERVATION_OK);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_OK);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        }
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
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        observation->set_result(
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
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

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest, CancelActions) {
  // Makes sure we are on about:blank so the browser won't open a new tab to
  // navigate.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());
  const GURL target_url = embedded_test_server()->GetURL("/title1.html");
  content::TestNavigationManager navigation_manager(web_contents(), target_url);

  optimization_guide::proto::Actions action = MakeNavigateForTaskId(
      active_tab()->GetHandle(), target_url.spec(), task_id);
  std::unique_ptr<AsyncActionWaiter> waiter = PerformActionsAsync(action);

  // WaitForRequestStart() also pauses the navigation.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kActing);
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_FALSE(navigation_manager.was_committed());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kReflecting);
  auto result = waiter->Wait();
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              HasResultCode(mojom::ActionResultCode::kActionsCancelled));
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       CancelActionsNoActionsToCancel) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kCreated);
  EXPECT_THAT(CancelActions(task_id),
              base::test::ValueIs(glic::mojom::CancelActionsResult::kSuccess));
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id)->GetState(),
            ActorTask::State::kCreated);
}

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTest,
                       LogsActorTaskCreatedOnCreateTask) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  EXPECT_NE(task_id, TaskId());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, true, 1);
}

class ActorFunctionalBrowserTestWithoutPolicyExemption
    : public ActorFunctionalBrowserTest {
 public:
  ActorFunctionalBrowserTestWithoutPolicyExemption() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "false"}}}},
        /*disabled_features=*/{});
  }
  ~ActorFunctionalBrowserTestWithoutPolicyExemption() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorFunctionalBrowserTestWithoutPolicyExemption,
                       LogsActorTaskFailedOnCreateTask) {
  base::HistogramTester histogram_tester;

  base::expected<TaskId, std::string> result = CreateTask();
  EXPECT_FALSE(result.has_value());

  constexpr std::string_view kActorTaskCreatedHistogram = "Actor.Task.Created";
  histogram_tester.ExpectUniqueSample(kActorTaskCreatedHistogram, false, 1);
}

class ActorPageContextMetricsTest : public ActorFunctionalBrowserTest {
 public:
  using ResultCallback =
      base::RepeatingCallback<void(size_t /*fetch_num*/,
                                   TabObservation*,
                                   const FetchPageContextResult&)>;
  void RunTestWithPageContextResult(ResultCallback result_callback) {
    ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
    ASSERT_NE(task_id, TaskId());

    // Perform an arbitrary action.
    ::optimization_guide::proto::Actions action =
        MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                  ::optimization_guide::proto::ClickAction::LEFT,
                  ::optimization_guide::proto::ClickAction::SINGLE);
    action.set_task_id(task_id.value());

    // Each test case provides its own faked/mocked result for the
    // TabObservation.
    ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
        [&, this](TabObservation* observation,
                  const FetchPageContextResult& fetch_result) {
          ++num_fetches_;
          result_callback.Run(num_fetches_, observation, fetch_result);
        }));

    auto result = PerformActions(action);

    ASSERT_TRUE(result.has_value());
  }

  void SuccessfulObservation(TabObservation* observation) {
    observation->set_result(TabObservation::TAB_OBSERVATION_OK);
    observation->set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_OK);
    observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
  }

  size_t num_fetches() const { return num_fetches_; }

 private:
  size_t num_fetches_ = 0;
};

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(kActorPageContextObservationOutcome,
                                      ActorObservationOutcome::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_SuccessAfterRetry) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
        } else {
          SuccessfulObservation(observation);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectUniqueSample(
      kActorPageContextObservationOutcome,
      ActorObservationOutcome::kSuccessAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       ObservationOutcomeMetrics_Failure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectUniqueSample(
      kActorPageContextObservationOutcome,
      ActorObservationOutcome::kFailureAfterRetry, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_Success) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        SuccessfulObservation(observation);
      }));

  ASSERT_EQ(num_fetches(), 1ul);

  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_APCFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        } else {
          SuccessfulObservation(observation);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Ensure we record a failure in APC (for initial failure) and a success (for
  // retry).
  histogram_tester.ExpectTotalCount(kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kApcError, 1);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_RepeatedAPCFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
        observation->set_annotated_page_content_result(
            TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
        observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Ensure we record two failures in APC since the retry fails as well.
  histogram_tester.ExpectUniqueSample(kActorPageContextTabObservationResult,
                                      ActorTabObservationResult::kApcError, 2);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_APCAndScreenshotFailure) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
        observation->set_annotated_page_content_result(
            TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
        observation->set_screenshot_result(TabObservation::SCREENSHOT_ERROR);
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  // Since both APC and screenshot had failures ensure the combined bucket is
  // used.
  histogram_tester.ExpectUniqueSample(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kApcAndScreenshotNotOk, 2);
}

IN_PROC_BROWSER_TEST_F(ActorPageContextMetricsTest,
                       TabObservationResult_MultipleFailures) {
  base::HistogramTester histogram_tester;

  RunTestWithPageContextResult(base::BindLambdaForTesting(
      [&](size_t fetch_num, TabObservation* observation,
          const FetchPageContextResult&) {
        if (fetch_num == 1) {
          observation->set_result(TabObservation::TAB_OBSERVATION_FETCH_ERROR);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_TIMEOUT);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        } else {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_WEB_CONTENTS_CHANGED);
        }
      }));

  ASSERT_EQ(num_fetches(), 2ul);

  histogram_tester.ExpectTotalCount(kActorPageContextTabObservationResult, 2);
  histogram_tester.ExpectBucketCount(kActorPageContextTabObservationResult,
                                     ActorTabObservationResult::kApcTimeout, 1);
  histogram_tester.ExpectBucketCount(
      kActorPageContextTabObservationResult,
      ActorTabObservationResult::kWebContentsChanged, 1);
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

IN_PROC_BROWSER_TEST_P(ActorFunctionalBrowserTestCreateActorTab,
                       CreateActorTabWithInvalidTask) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1u);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  TaskId invalid_task_id = actor::TaskId(task_id.value().value() + 100);

  // Create a new tab with an invalid task id.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(invalid_task_id, /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));

  // CreateActorTab should have returned an error;
  EXPECT_FALSE(new_tab_handler.has_value());

  // Verify it is bound to the task.
  EXPECT_TRUE(
      actor_keyed_service()->GetTask(task_id.value())->GetTabs().empty());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ActorFunctionalBrowserTestCreateActorTab,
    ::testing::Values(GURL(chrome::kChromeUINewTabURL),
                      GURL(url::kAboutBlankURL)));

}  // namespace
}  // namespace actor
