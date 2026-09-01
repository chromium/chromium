// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_actions_runner.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace actor {

namespace {

void SetTabIdIfMissing(optimization_guide::proto::Action& action,
                       int32_t tab_id) {
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kClick:
      if (!action.click().has_tab_id()) {
        action.mutable_click()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kType:
      if (!action.type().has_tab_id()) {
        action.mutable_type()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kScroll:
      if (!action.scroll().has_tab_id()) {
        action.mutable_scroll()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kMoveMouse:
      if (!action.move_mouse().has_tab_id()) {
        action.mutable_move_mouse()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kDragAndRelease:
      if (!action.drag_and_release().has_tab_id()) {
        action.mutable_drag_and_release()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kSelect:
      if (!action.select().has_tab_id()) {
        action.mutable_select()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kNavigate:
      if (!action.navigate().has_tab_id()) {
        action.mutable_navigate()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kBack:
      if (!action.back().has_tab_id()) {
        action.mutable_back()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kForward:
      if (!action.forward().has_tab_id()) {
        action.mutable_forward()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kCloseTab:
      if (!action.close_tab().has_tab_id()) {
        action.mutable_close_tab()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kActivateTab:
      if (!action.activate_tab().has_tab_id()) {
        action.mutable_activate_tab()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kAttemptLogin:
      if (!action.attempt_login().has_tab_id()) {
        action.mutable_attempt_login()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kAttemptFormFilling:
      if (!action.attempt_form_filling().has_tab_id()) {
        action.mutable_attempt_form_filling()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kAttemptOtpFilling:
      if (!action.attempt_otp_filling().has_tab_id()) {
        action.mutable_attempt_otp_filling()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kTranslatePage:
      if (!action.translate_page().has_tab_id()) {
        action.mutable_translate_page()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kScriptTool:
      if (!action.script_tool().has_tab_id()) {
        action.mutable_script_tool()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kScrollTo:
      if (!action.scroll_to().has_tab_id()) {
        action.mutable_scroll_to()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kMediaControl:
      if (!action.media_control().has_tab_id()) {
        action.mutable_media_control()->set_tab_id(tab_id);
      }
      break;
    case optimization_guide::proto::Action::kLoadAndExtractContent:
    case optimization_guide::proto::Action::kWait:
    case optimization_guide::proto::Action::kCreateTab:
    case optimization_guide::proto::Action::kCreateWindow:
    case optimization_guide::proto::Action::kCloseWindow:
    case optimization_guide::proto::Action::kActivateWindow:
    case optimization_guide::proto::Action::kYieldToUser:
    case optimization_guide::proto::Action::ACTION_NOT_SET:
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

}  // namespace

ActorActionsRunner::ActorActionsRunner(
    Profile& profile,
    TaskSourceInfo source_info,
    optimization_guide::proto::Actions actions,
    base::OnceClosure on_complete,
    int32_t tab_id)
    : profile_(profile),
      source_info_(std::move(source_info)),
      actions_(std::move(actions)),
      on_complete_(std::move(on_complete)),
      tab_id_(tab_id) {}

ActorActionsRunner::~ActorActionsRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopTaskIfActive();
}

void ActorActionsRunner::StopTaskIfActive() {
  if (task_id_) {
    if (auto* actor_service = ActorKeyedService::Get(&profile_.get())) {
      actor_service->StopTask(*task_id_,
                              ActorTask::StoppedReason::kTaskComplete);
    }
    task_id_ = std::nullopt;
    task_state_changed_subscription_ = {};
  }
}

void ActorActionsRunner::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_started_);
  is_started_ = true;

  if (tab_id_ != 0) {
    for (int i = 0; i < actions_.actions_size(); ++i) {
      SetTabIdIfMissing(*actions_.mutable_actions(i), tab_id_);
    }
  }

  auto* actor_service = ActorKeyedService::Get(&profile_.get());
  if (!actor_service) {
    LOG(ERROR) << "ActorKeyedService not available.";
    Finish(std::make_unique<optimization_guide::proto::ActionsResult>(
        BuildErrorActionsResult(mojom::ActionResultCode::kTaskWentAway,
                                std::nullopt)));
    return;
  }

  auto options = webui::mojom::TaskOptions::New();
  options->duration = webui::mojom::TaskDuration::kTransient;
  // ActorActionsRunner executes transient actions on behalf of trusted browser
  // features and extension APIs, which do not enforce enterprise policy URL
  // blocking or content validation at the actor task level.
  task_id_ = actor_service->CreateTaskWithOptions(
      source_info_, GetNullEnterprisePolicyChecker(), std::move(options),
      nullptr);
  if (!task_id_) {
    LOG(ERROR) << "Failed to create Actor task.";
    Finish(std::make_unique<optimization_guide::proto::ActionsResult>(
        BuildErrorActionsResult(mojom::ActionResultCode::kArgumentsInvalid,
                                std::nullopt)));
    return;
  }

  if (auto* task = actor_service->GetTask(*task_id_)) {
    actor_task_ = task->GetWeakPtr();
  }

  task_state_changed_subscription_ =
      actor_service->AddTaskStateChangedCallback(base::BindRepeating(
          &ActorActionsRunner::OnTaskStateChanged, base::Unretained(this)));

  auto build_result = BuildToolRequest(actions_);
  if (!build_result.has_value()) {
    LOG(ERROR) << "BuildToolRequest failed at action index: "
               << build_result.error().first << " with error code: "
               << static_cast<int>(build_result.error().second);
    Finish(std::make_unique<optimization_guide::proto::ActionsResult>(
        BuildErrorActionsResult(build_result.error().second,
                                build_result.error().first)));
    return;
  }

  ActorTaskMetadata task_metadata(actions_);

  actor_service->PerformActions(
      *task_id_, std::move(build_result.value()), std::move(task_metadata),
      base::BindOnce(&ActorActionsRunner::OnActionsPerformed,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     actions_.skip_async_observation_collection(),
                     GetScreenshotCollectionOptions(actions_)));
}

std::unique_ptr<optimization_guide::proto::ActionsResult>
ActorActionsRunner::TakeResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(result_);
}

const optimization_guide::proto::ActionsResult* ActorActionsRunner::result()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return result_.get();
}

void ActorActionsRunner::OnTaskStateChanged(ActorTask& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If transient tasks need to handle elicitation events (e.g.
  // ActorTask::State::kWaitingOnUser) or notify callers of intermediate user
  // interactions, observe those transitions here.
  if (task.IsCompleted() && task_id_ == task.id()) {
    task_id_ = std::nullopt;
    task_state_changed_subscription_ = {};
  }
}

void ActorActionsRunner::OnActionsPerformed(
    base::TimeTicks start_time,
    bool skip_async_observation_collection,
    std::optional<page_content_annotations::ScreenshotOptions::
                      ScreenshotCollectionOptions> screenshot_options,
    std::vector<ActionResultWithLatencyInfo> action_results,
    TabObservationStrategy observation_strategy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* actor_service = ActorKeyedService::Get(&profile_.get());
  auto* task =
      (actor_service && task_id_) ? actor_service->GetTask(*task_id_) : nullptr;
  if (!task) {
    Finish(std::make_unique<optimization_guide::proto::ActionsResult>(
        BuildErrorActionsResult(mojom::ActionResultCode::kTaskWentAway,
                                std::nullopt)));
    return;
  }

  BuildActionsResultWithObservations(
      profile_.get(), start_time, std::move(action_results), *task,
      skip_async_observation_collection, screenshot_options,
      base::BindOnce(&ActorActionsRunner::OnActionsResultBuilt,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorActionsRunner::OnActionsResultBuilt(
    base::TimeTicks start_time,
    std::vector<ActionResultWithLatencyInfo> action_results,
    TaskId task_id,
    bool skip_async_observation_information,
    std::optional<page_content_annotations::ScreenshotOptions::
                      ScreenshotCollectionOptions>
        screenshot_collection_options,
    std::unique_ptr<optimization_guide::proto::ActionsResult> result,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    result = std::make_unique<optimization_guide::proto::ActionsResult>(
        BuildErrorActionsResult(mojom::ActionResultCode::kTaskWentAway,
                                std::nullopt));
  }
  Finish(std::move(result));
}

void ActorActionsRunner::Finish(
    std::unique_ptr<optimization_guide::proto::ActionsResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  result_ = std::move(result);
  StopTaskIfActive();

  if (on_complete_) {
    std::move(on_complete_).Run();
  }
}

}  // namespace actor
