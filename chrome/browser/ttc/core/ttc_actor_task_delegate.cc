// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_actor_task_delegate.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/common/actor_webui.mojom.h"

namespace ttc {

namespace webui_mojom = actor::webui::mojom;

TtcActorTaskDelegate::TtcActorTaskDelegate() = default;

TtcActorTaskDelegate::~TtcActorTaskDelegate() = default;

base::WeakPtr<TtcActorTaskDelegate> TtcActorTaskDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TtcActorTaskDelegate::OnTabAddedToTask(
    actor::TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {}

void TtcActorTaskDelegate::OnTaskTabsVisibilityChanged(actor::TaskId task_id,
                                                       bool has_visible_tab) {}

void TtcActorTaskDelegate::RequestToShowCredentialSelectionDialog(
    actor::TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials,
    CredentialSelectedCallback callback) {
  // Only requested by the attempt_login tool, which TTC doesn't expose.
  NOTREACHED();
}

void TtcActorTaskDelegate::RequestToShowUserConfirmationDialog(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    UserConfirmationDialogCallback callback) {
  // An error is reported rather than a denial, since the user was never asked.
  auto response = webui_mojom::UserConfirmationDialogResponse::New();
  response->result = webui_mojom::ConfirmationRequestResult::NewErrorReason(
      webui_mojom::ConfirmationRequestErrorReason::kRequestPromiseNoSubscriber);
  std::move(callback).Run(std::move(response));
}

void TtcActorTaskDelegate::RequestToConfirmNavigation(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    NavigationConfirmationCallback callback) {
  auto response = webui_mojom::NavigationConfirmationResponse::New();
  response->result =
      webui_mojom::ConfirmationRequestResult::NewPermissionGranted(true);
  std::move(callback).Run(std::move(response));
}

void TtcActorTaskDelegate::RequestToShowAutofillSuggestionsDialog(
    actor::TaskId task_id,
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
    AutofillSuggestionSelectedCallback callback) {
  // Only requested by the attempt_form_filling tool, which TTC doesn't expose.
  NOTREACHED();
}

void TtcActorTaskDelegate::RequestToShowGmailOtpOptInDialog(
    actor::TaskId task_id,
    GmailOtpOptInCallback callback) {
  // Only requested by the attempt_otp_filling tool, which TTC doesn't expose.
  NOTREACHED();
}

void TtcActorTaskDelegate::RequestToShowGmailOtpConfirmationDialog(
    actor::TaskId task_id,
    const std::string& verification_code,
    GmailOtpConfirmationCallback callback) {
  // Only requested by the attempt_otp_filling tool, which TTC doesn't expose.
  NOTREACHED();
}

}  // namespace ttc
