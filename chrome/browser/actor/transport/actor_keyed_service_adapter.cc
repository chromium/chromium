// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

ActorKeyedServiceAdapter::ActorKeyedServiceAdapter(
    ActorKeyedService* actor_service)
    : actor_service_(actor_service) {
  CHECK(actor_service_);
}

ActorKeyedServiceAdapter::~ActorKeyedServiceAdapter() = default;

void ActorKeyedServiceAdapter::StartTask(
    const std::string& session_id,
    const optimization_guide::proto::BrowserStartTask& request,
    StartTaskCallback callback) {
  auto options = webui::mojom::TaskOptions::New();
  if (request.tab_id() > 0) {
    options->actuation_tab_id = request.tab_id();
  }

  // TODO(crbug.com/568105613): Use a non-null enterprise policy checker.
  TaskId task_id = actor_service_->CreateTaskWithOptions(
      TaskSourceInfo(TaskSourceInfo::Client::kBrowserActuator, session_id),
      GetNullEnterprisePolicyChecker(), std::move(options),
      weak_ptr_factory_.GetWeakPtr(),
      ui::ActorUiStateManager::Get(actor_service_->GetProfile()));

  optimization_guide::proto::BrowserStartTaskResult result;
  result.set_task_id(task_id.value());
  if (request.tab_id() > 0) {
    result.set_tab_id(request.tab_id());
  }
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  std::move(callback).Run(std::move(result));
}

void ActorKeyedServiceAdapter::StopTask(TaskId task_id,
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

void ActorKeyedServiceAdapter::Act(
    const optimization_guide::proto::Actions& actions,
    ActCallback callback) {
  // TODO(crbug.com/565390794): Pipe Act to ActorKeyedService.
  NOTIMPLEMENTED();
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
