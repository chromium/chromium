// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/types/optional_util.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {
namespace {

ActorTask::StoppedReason ToStoppedReason(
    proto::StopTaskRequest::Reason reason) {
  switch (reason) {
    case proto::StopTaskRequest::USER_CANCELLED:
      return ActorTask::StoppedReason::kStoppedByUser;
    case proto::StopTaskRequest::TIMEOUT:
      return ActorTask::StoppedReason::kTimeout;
    case proto::StopTaskRequest::POLICY_ABORT:
      return ActorTask::StoppedReason::kAbortedByPolicy;
    case proto::StopTaskRequest::REASON_UNSPECIFIED:
    case proto::StopTaskRequest::TASK_COMPLETED:
    default:
      return ActorTask::StoppedReason::kTaskComplete;
  }
}

proto::PerformActionsResponse::Status ToPerformActionsStatus(
    int32_t action_result) {
  switch (static_cast<mojom::ActionResultCode>(action_result)) {
    case mojom::ActionResultCode::kOk:
      return proto::PerformActionsResponse::STATUS_OK;
    case mojom::ActionResultCode::kActionsCancelled:
      return proto::PerformActionsResponse::STATUS_TASK_CANCELLED;
    case mojom::ActionResultCode::kTaskPaused:
      return proto::PerformActionsResponse::STATUS_TASK_PAUSED_BY_USER;
    case mojom::ActionResultCode::kTaskWentAway:
    case mojom::ActionResultCode::kTabWentAway:
      return proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED;
    case mojom::ActionResultCode::kUrlBlocked:
      return proto::PerformActionsResponse::STATUS_SECURITY_VIOLATION;
    default:
      return proto::PerformActionsResponse::STATUS_ACTION_FAILED;
  }
}

proto::BrowserObservation::Status ToBrowserObservationStatus(
    optimization_guide::proto::TabObservation::TabObservationResult result) {
  switch (result) {
    case optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK:
    case optimization_guide::proto::TabObservation::TAB_OBSERVATION_FETCH_ERROR:
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_SCREENSHOT_TIMEOUT:
      return proto::BrowserObservation::STATUS_OBSERVED;
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_TAB_WENT_AWAY:
      return proto::BrowserObservation::STATUS_TAB_WENT_AWAY;
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_PAGE_CRASHED:
      return proto::BrowserObservation::STATUS_PAGE_CRASHED;
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_WEB_CONTENTS_CHANGED:
      return proto::BrowserObservation::STATUS_WEB_CONTENTS_CHANGED;
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE:
      return proto::BrowserObservation::STATUS_NOT_ELIGIBLE;
    case optimization_guide::proto::TabObservation::
        TAB_OBSERVATION_UNKNOWN_ERROR:
    default:
      return proto::BrowserObservation::STATUS_UNKNOWN_ERROR;
  }
}

proto::ObservationError::Code ToScreenshotErrorCode(
    const optimization_guide::proto::TabObservation& tab_obs) {
  if (tab_obs.result() == optimization_guide::proto::TabObservation::
                              TAB_OBSERVATION_SCREENSHOT_TIMEOUT ||
      tab_obs.screenshot_result() ==
          optimization_guide::proto::TabObservation::SCREENSHOT_TIMEOUT) {
    return proto::ObservationError::CODE_TIMEOUT;
  }
  return proto::ObservationError::CODE_FAILED;
}

proto::ObservationError::Code ToPageContentErrorCode(
    const optimization_guide::proto::TabObservation& tab_obs) {
  if (tab_obs.annotated_page_content_result() ==
      optimization_guide::proto::TabObservation::
          ANNOTATED_PAGE_CONTENT_TIMEOUT) {
    return proto::ObservationError::CODE_TIMEOUT;
  }
  return proto::ObservationError::CODE_FAILED;
}

bool ShouldSkipAsyncObservationCollection(
    proto::ObservationConfig::ObservationLevel level) {
  switch (level) {
    case proto::ObservationConfig::OBSERVATION_LEVEL_UNSPECIFIED:
    case proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_SCREENSHOT_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_APC_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_FULL:
      return false;
    case proto::ObservationConfig::OBSERVATION_LEVEL_NONE:
    default:
      return true;
  }
}

bool ShouldIncludeObservations(
    proto::ObservationConfig::ObservationLevel level) {
  switch (level) {
    case proto::ObservationConfig::OBSERVATION_LEVEL_UNSPECIFIED:
    case proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_SCREENSHOT_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_APC_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_FULL:
      return true;
    case proto::ObservationConfig::OBSERVATION_LEVEL_NONE:
    default:
      return false;
  }
}

bool ShouldIncludeScreenshot(proto::ObservationConfig::ObservationLevel level) {
  switch (level) {
    case proto::ObservationConfig::OBSERVATION_LEVEL_UNSPECIFIED:
    case proto::ObservationConfig::OBSERVATION_LEVEL_SCREENSHOT_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_FULL:
      return true;
    case proto::ObservationConfig::OBSERVATION_LEVEL_NONE:
    case proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_APC_ONLY:
    default:
      return false;
  }
}

bool ShouldIncludePageContent(
    proto::ObservationConfig::ObservationLevel level) {
  switch (level) {
    case proto::ObservationConfig::OBSERVATION_LEVEL_UNSPECIFIED:
    case proto::ObservationConfig::OBSERVATION_LEVEL_APC_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_FULL:
      return true;
    case proto::ObservationConfig::OBSERVATION_LEVEL_NONE:
    case proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY:
    case proto::ObservationConfig::OBSERVATION_LEVEL_SCREENSHOT_ONLY:
    default:
      return false;
  }
}

}  // namespace

ActorKeyedServiceAdapter::ActorKeyedServiceAdapter(
    ActorKeyedService* actor_service,
    const EnterprisePolicyChecker* enterprise_policy_checker)
    : actor_service_(actor_service),
      enterprise_policy_checker_(enterprise_policy_checker) {
  CHECK(actor_service_);
  CHECK(enterprise_policy_checker_);
}

ActorKeyedServiceAdapter::~ActorKeyedServiceAdapter() = default;

void ActorKeyedServiceAdapter::StartTask(const proto::StartTaskRequest& request,
                                         StartTaskCallback callback) {
  auto options = webui::mojom::TaskOptions::New();
  if (!request.task_title().empty()) {
    options->title = request.task_title();
  }

  proto::StartTaskResponse response;
  // TODO(crbug.com/576384635): Support binding multiple initial target surfaces
  // and failing task creation on unbindable tabs in ActorKeyedService.
  if (!request.target_surfaces().empty()) {
    const proto::TargetSurface& surface = request.target_surfaces(0);
    if (!surface.has_tab_id() || surface.tab_id() <= 0 ||
        !actor_service_->GetSurfaceRegistry().GetForTab(
            tabs::TabHandle(surface.tab_id()))) {
      response.set_status(
          proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
      std::move(callback).Run(TaskId(), std::move(response));
      return;
    }
    options->actuation_tab_id = surface.tab_id();
    proto::StartTaskResponse::BoundSurface* bound_surface =
        response.add_bound_surfaces();
    bound_surface->set_tab_id(surface.tab_id());
    bound_surface->set_execution_mode(surface.execution_mode());
  }

  TaskId task_id = actor_service_->CreateTaskWithOptions(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator,
                     /*id=*/std::nullopt),
      enterprise_policy_checker_, std::move(options),
      weak_ptr_factory_.GetWeakPtr(),
      ui::ActorUiStateManager::Get(actor_service_->GetProfile()));

  response.set_status(proto::StartTaskResponse::STATUS_OK);
  std::move(callback).Run(task_id, std::move(response));
}

void ActorKeyedServiceAdapter::StopTask(TaskId task_id,
                                        const proto::StopTaskRequest& request,
                                        StopTaskCallback callback) {
  if (!actor_service_->GetTask(task_id)) {
    std::move(callback).Run(false);
    return;
  }

  actor_service_->StopTask(task_id, ToStoppedReason(request.reason()));
  std::move(callback).Run(true);
}

void ActorKeyedServiceAdapter::Act(TaskId task_id,
                                   const proto::PerformActionsRequest& request,
                                   ActCallback callback) {
  optimization_guide::proto::Actions actions =
      BuildActionsProto(task_id, request);
  std::optional<proto::ObservationConfig> observation_config;
  if (request.has_observation_config()) {
    observation_config = request.observation_config();
  }
  actor_keyed_service_proto_wrapper_.PerformActions(
      actions,
      base::BindOnce(
          [](std::optional<proto::ObservationConfig> observation_config,
             ActCallback callback,
             optimization_guide::proto::ActionsResult result) {
            std::move(callback).Run(BuildPerformActionsResponse(
                std::move(result), base::OptionalToPtr(observation_config)));
          },
          std::move(observation_config), std::move(callback)));
}

// static
optimization_guide::proto::Actions ActorKeyedServiceAdapter::BuildActionsProto(
    TaskId task_id,
    const proto::PerformActionsRequest& request) {
  optimization_guide::proto::Actions actions;
  actions.set_task_id(task_id.value());
  *actions.mutable_actions() = request.actions();
  if (request.has_observation_config() &&
      ShouldSkipAsyncObservationCollection(
          request.observation_config().level())) {
    actions.set_skip_async_observation_collection(true);
  }
  if (request.has_agent_container_config()) {
    *actions.mutable_task_metadata()
         ->mutable_security()
         ->mutable_agent_container_config() = request.agent_container_config();
  }
  return actions;
}

// static
proto::PerformActionsResponse
ActorKeyedServiceAdapter::BuildPerformActionsResponse(
    optimization_guide::proto::ActionsResult result,
    const proto::ObservationConfig* observation_config) {
  proto::PerformActionsResponse response;
  response.set_status(ToPerformActionsStatus(result.action_result()));
  response.set_action_status_code(result.action_result());
  if (result.has_index_of_failed_action() &&
      result.index_of_failed_action() >= 0) {
    response.set_failed_action_index(result.index_of_failed_action());
  }
  for (std::string& info : *result.mutable_extra_information()) {
    response.add_extra_information(std::move(info));
  }

  proto::ObservationConfig::ObservationLevel level =
      proto::ObservationConfig::OBSERVATION_LEVEL_FULL;
  if (observation_config) {
    level = observation_config->level();
  }
  if (ShouldIncludeObservations(level)) {
    for (optimization_guide::proto::TabObservation& tab_obs :
         *result.mutable_tabs()) {
      proto::BrowserObservation* browser_obs = response.add_observations();
      browser_obs->set_tab_id(tab_obs.id());
      if (tab_obs.has_annotated_page_content() &&
          tab_obs.annotated_page_content().has_main_frame_data()) {
        browser_obs->set_current_url(
            tab_obs.annotated_page_content().main_frame_data().url());
      }
      browser_obs->set_status(ToBrowserObservationStatus(tab_obs.result()));
      if (browser_obs->status() != proto::BrowserObservation::STATUS_OBSERVED) {
        continue;
      }

      if (ShouldIncludeScreenshot(level)) {
        if (tab_obs.has_screenshot() && !tab_obs.screenshot().empty()) {
          proto::BinaryData* screenshot_data =
              browser_obs->mutable_screenshot_data();
          screenshot_data->set_inline_data(
              std::move(*tab_obs.mutable_screenshot()));
          if (tab_obs.has_screenshot_mime_type()) {
            screenshot_data->set_mime_type(
                std::move(*tab_obs.mutable_screenshot_mime_type()));
          }
        } else {
          browser_obs->mutable_screenshot_error()->set_code(
              ToScreenshotErrorCode(tab_obs));
        }
      }

      if (ShouldIncludePageContent(level)) {
        if (tab_obs.has_annotated_page_content()) {
          *browser_obs->mutable_inline_annotated_page_content() =
              std::move(*tab_obs.mutable_annotated_page_content());
        } else {
          browser_obs->mutable_page_content_error()->set_code(
              ToPageContentErrorCode(tab_obs));
        }
      }
    }
  }

  return response;
}

void ActorKeyedServiceAdapter::OnTabAddedToTask(
    TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {}

void ActorKeyedServiceAdapter::OnTaskTabsVisibilityChanged(
    TaskId task_id,
    bool has_visible_tab) {}

void ActorKeyedServiceAdapter::RequestToShowCredentialSelectionDialog(
    TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials,
    CredentialSelectedCallback callback) {
  // TODO(crbug.com/565389892): Forward credential selection request over
  // transport.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::RequestToShowUserConfirmationDialog(
    TaskId task_id,
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    UserConfirmationDialogCallback callback) {
  // TODO(crbug.com/565389892): Forward user confirmation request over
  // transport.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::RequestToConfirmNavigation(
    TaskId task_id,
    const url::Origin& navigation_origin,
    NavigationConfirmationCallback callback) {
  // TODO(crbug.com/565389892): Forward navigation confirmation request over
  // transport.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::RequestToShowAutofillSuggestionsDialog(
    TaskId task_id,
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
    AutofillSuggestionSelectedCallback callback) {
  // TODO(crbug.com/565389892): Forward autofill suggestions request over
  // transport.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::RequestToShowGmailOtpOptInDialog(
    TaskId task_id,
    GmailOtpOptInCallback callback) {
  // TODO(crbug.com/565389892): Forward Gmail OTP opt-in request over transport.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::RequestToShowGmailOtpConfirmationDialog(
    TaskId task_id,
    const std::string& verification_code,
    GmailOtpConfirmationCallback callback) {
  // TODO(crbug.com/565389892): Forward Gmail OTP confirmation request over
  // transport.
  NOTIMPLEMENTED();
}

}  // namespace actor
