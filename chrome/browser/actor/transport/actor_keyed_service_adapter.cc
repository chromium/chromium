// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

ActorKeyedServiceAdapter::ActorKeyedServiceAdapter(
    ActorKeyedService* actor_service) {
  // TODO(crbug.com/565390654): Store and validate actor_service.
  NOTIMPLEMENTED();
}

ActorKeyedServiceAdapter::~ActorKeyedServiceAdapter() = default;

void ActorKeyedServiceAdapter::StartTask(
    const std::string& session_id,
    const optimization_guide::proto::BrowserStartTask& request,
    StartTaskCallback callback) {
  // TODO(crbug.com/565390654): Pipe StartTask to ActorKeyedService.
  NOTIMPLEMENTED();
}

void ActorKeyedServiceAdapter::StopTask(TaskId task_id,
                                        StopTaskCallback callback) {
  // TODO(crbug.com/565390793): Pipe StopTask to ActorKeyedService.
  NOTIMPLEMENTED();
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
