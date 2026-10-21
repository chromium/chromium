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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

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

  TaskId task_id = actor_service_->CreateTaskWithOptions(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator,
                     /*id=*/std::nullopt),
      enterprise_policy_checker_, std::move(options),
      weak_ptr_factory_.GetWeakPtr(),
      ui::ActorUiStateManager::Get(actor_service_->GetProfile()));

  // TODO(crbug.com/571106281): Bind target_surfaces on StartTask.
  proto::StartTaskResponse response;
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

  // TODO(crbug.com/571106281): Update StoppedReason to one provided by the
  // downstream message.
  actor_service_->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
  std::move(callback).Run(true);
}

void ActorKeyedServiceAdapter::Act(TaskId task_id,
                                   const proto::PerformActionsRequest& request,
                                   ActCallback callback) {
  optimization_guide::proto::Actions actions;
  actions.set_task_id(task_id.value());
  *actions.mutable_actions() = request.actions();
  // TODO(crbug.com/571106281): Apply observation_config and populate
  // observations in PerformActionsResponse.
  actor_keyed_service_proto_wrapper_.PerformActions(
      actions,
      base::BindOnce(
          [](ActCallback callback,
             optimization_guide::proto::ActionsResult result) {
            proto::PerformActionsResponse response;
            response.set_status(
                result.action_result() ==
                        static_cast<int32_t>(mojom::ActionResultCode::kOk)
                    ? proto::PerformActionsResponse::STATUS_OK
                    : proto::PerformActionsResponse::STATUS_ACTION_FAILED);
            response.set_action_status_code(result.action_result());
            if (result.has_index_of_failed_action() &&
                result.index_of_failed_action() >= 0) {
              response.set_failed_action_index(result.index_of_failed_action());
            }
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
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
