// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_proto_conversion.h"

namespace glic::actor {

ScopedMockTabObservationResult::ScopedMockTabObservationResult(
    base::RepeatingCallback<void(TabObservation*,
                                 const FetchPageContextResult&)> callback) {
  ::actor::SetTabObservationResultOverrideForTesting(callback);
}

ScopedMockTabObservationResult::~ScopedMockTabObservationResult() {
  ::actor::SetTabObservationResultOverrideForTesting(
      base::RepeatingCallback<void(TabObservation*,
                                   const FetchPageContextResult&)>());
}

AsyncActionWaiter::AsyncActionWaiter(content::RenderFrameHost* rfh,
                                     std::string request_id)
    : queue_(rfh), request_id_(std::move(request_id)) {}

base::expected<ActionsResult, std::string> AsyncActionWaiter::Wait() {
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

    return ::actor::ParseBase64Proto<ActionsResult>(*result_base64);
  }
}

GlicActorFunctionalBrowserTestBase::GlicActorFunctionalBrowserTestBase() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kGlicMultiInstance, {}},
                            {::actor::kActorBindCreatedTabToTask, {}},
                            {features::kGlicActor,
                             {{features::kGlicActorPolicyControlExemption.name,
                               "true"}}}},
      /*disabled_features=*/{});
}

GlicActorFunctionalBrowserTestBase::~GlicActorFunctionalBrowserTestBase() =
    default;

::actor::ActorKeyedService*
GlicActorFunctionalBrowserTestBase::actor_keyed_service() {
  return ::actor::ActorKeyedService::Get(browser()->profile());
}

void GlicActorFunctionalBrowserTestBase::SetUpOnMainThread() {
  GlicFunctionalBrowserTestBase::SetUpOnMainThread();
  RunTestSequence(OpenGlic());
}

base::CallbackListSubscription
GlicActorFunctionalBrowserTestBase::CreateTaskCompletionSubscription(
    TaskId for_task_id,
    TestFuture<ActorTask::State>& future) {
  return actor_keyed_service()->AddTaskStateChangedCallback(
      base::BindLambdaForTesting(
          [&future, for_task_id](TaskId task_id, ActorTask::State state) {
            if (task_id == for_task_id && ActorTask::IsCompletedState(state)) {
              future.SetValue(state);
            }
          }));
}

ActorTask::State GlicActorFunctionalBrowserTestBase::GetActorTaskState(
    TaskId task_id) {
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  CHECK_NE(task, nullptr) << "ActorTask " << task_id << " not found.";
  return task->GetState();
}

base::expected<glic::mojom::CancelActionsResult, std::string>
GlicActorFunctionalBrowserTestBase::CancelActions(TaskId task_id) {
  std::string script = "window.client.browser.cancelActions($1)";
  ASSIGN_OR_RETURN(int result_int, EvalJsInGlicForInt(content::JsReplace(
                                       script, task_id.value())));
  return base::ok(static_cast<glic::mojom::CancelActionsResult>(result_int));
}

base::expected<TaskId, std::string>
GlicActorFunctionalBrowserTestBase::CreateTask(
    ::actor::webui::mojom::TaskOptionsPtr options) {
  std::string title;
  if (options && options->title) {
    title = content::JsReplace("{title: $1}", *options->title);
  }

  std::string script =
      base::StrCat({"window.client.browser.createTask(", title, ");"});
  ASSIGN_OR_RETURN(int task_id_int, EvalJsInGlicForInt(script));
  return base::ok(TaskId(task_id_int));
}

base::expected<ActionsResult, std::string>
GlicActorFunctionalBrowserTestBase::PerformActions(const Actions& actions) {
  return PerformActionsAsync(actions)->Wait();
}

std::unique_ptr<AsyncActionWaiter>
GlicActorFunctionalBrowserTestBase::PerformActionsAsync(
    const Actions& actions) {
  // TODO(crbug.com/471254787): Revise PerformActionsAsync to handle async JS
  // calls in a blocking manner in C++.
  std::string serialized_actions;
  CHECK(actions.SerializeToString(&serialized_actions));
  const std::string proto_base64 = base::Base64Encode(serialized_actions);

  static int counter = 0;
  std::string request_id = base::NumberToString(++counter);
  auto waiter =
      std::make_unique<AsyncActionWaiter>(FindGlicGuestMainFrame(), request_id);
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

void GlicActorFunctionalBrowserTestBase::StopActorTask(
    TaskId task_id,
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

void GlicActorFunctionalBrowserTestBase::PauseActorTask(
    TaskId task_id,
    glic::mojom::ActorTaskPauseReason pause_reason,
    tabs::TabHandle tab_handle) {
  base::expected<base::Value, std::string> pause_task_js_result = [&]() {
    if (tab_handle == tabs::TabHandle::Null()) {
      std::string script = "window.client.browser.pauseActorTask($1, $2);";
      return EvalJsInGlic(content::JsReplace(script, task_id.value(),
                                             static_cast<int>(pause_reason)));
    } else {
      std::string script = "window.client.browser.pauseActorTask($1, $2, $3);";
      return EvalJsInGlic(content::JsReplace(
          script, task_id.value(), static_cast<int>(pause_reason),
          base::NumberToString(tab_handle.raw_value())));
    }
  }();

  EXPECT_TRUE(pause_task_js_result.has_value())
      << "pauseActorTask() failed: " << pause_task_js_result.error();
}

base::expected<::actor::mojom::ActionResultCode, std::string>
GlicActorFunctionalBrowserTestBase::ResumeActorTask(
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
  return base::ok(
      static_cast<::actor::mojom::ActionResultCode>(action_result_int));
}

void GlicActorFunctionalBrowserTestBase::InterruptActorTask(TaskId task_id) {
  std::string script = "window.client.browser.interruptActorTask($1);";
  EXPECT_OK(EvalJsInGlic(content::JsReplace(script, task_id.value())));
}

void GlicActorFunctionalBrowserTestBase::UninterruptActorTask(TaskId task_id) {
  std::string script = "window.client.browser.uninterruptActorTask($1);";
  EXPECT_OK(EvalJsInGlic(content::JsReplace(script, task_id.value())));
}

void GlicActorFunctionalBrowserTestBase::WaitForTaskState(
    TaskId task_id,
    ActorTask::State expected_state) {
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

base::expected<tabs::TabHandle, std::string>
GlicActorFunctionalBrowserTestBase::CreateActorTab(
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
          open_in_background ? base::Value(*open_in_background) : base::Value(),
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

}  // namespace glic::actor
