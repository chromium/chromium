// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/security_token_request_controller.h"

#include <string>
#include <utility>

#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/login_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "chromeos/components/security_token_pin/error_generator.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

std::u16string GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_SECURITY_TOKEN_REQUEST_DIALOG_TITLE);
}

std::u16string GetDescription() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_SECURITY_TOKEN_REQUEST_DIALOG_DESCRIPTION);
}

std::u16string GetAccessibleTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_SECURITY_TOKEN_REQUEST_DIALOG_TITLE);
}

}  // namespace

SecurityTokenRequestController::SecurityTokenRequestController() = default;

SecurityTokenRequestController::~SecurityTokenRequestController() {
  ClosePinUi();
}

void SecurityTokenRequestController::ResetRequestCanceled() {
  request_canceled_ = false;
}

PinRequestView::SubmissionResult SecurityTokenRequestController::OnPinSubmitted(
    const std::string& code) {
  if (!on_pin_submitted_.is_null()) {
    std::move(on_pin_submitted_).Run(code);
  }
  return PinRequestView::SubmissionResult::kSubmitPending;
}

void SecurityTokenRequestController::OnBack() {
  request_canceled_ = true;
  if (!on_canceled_by_user_.is_null()) {
    std::move(on_canceled_by_user_).Run();
  }
  ClosePinUi();
}

void SecurityTokenRequestController::OnHelp() {
  NOTREACHED();
}

bool SecurityTokenRequestController::SetPinUiState(
    SecurityTokenPinRequest request) {
  // Unable to request a PIN while the PinRequestWidget is already used for
  // something that is not a SecurityTokenPinRequest.
  // Also deny the request when the user has just canceled another request: For
  // example, logging in with smart cards usually requires two requests for the
  // same PIN. When the user has canceled the first one, we do not show another
  // right afterwards.
  if ((PinRequestWidget::Get() && !security_token_request_in_progress_) ||
      request_canceled_) {
    std::move(request.pin_ui_closed_callback).Run();
    return false;
  }

  on_pin_submitted_ = std::move(request.pin_entered_callback);
  on_canceled_by_user_ = std::move(request.pin_ui_closed_callback);

  // If this is a new request, open a PIN widget. Otherwise, just update the
  // existing widget.
  if (!security_token_request_in_progress_) {
    security_token_request_in_progress_ = true;
    PinRequest pin_request;
    pin_request.on_pin_request_done = base::DoNothing();
    pin_request.pin_keyboard_always_enabled = true;
    pin_request.extra_dimmer = true;
    pin_request.title = GetTitle();
    pin_request.description = GetDescription();
    pin_request.accessible_title = GetAccessibleTitle();
    PinRequestWidget::Show(std::move(pin_request), this);
  }

  PinRequestWidget::Get()->ClearInput();
  PinRequestWidget::Get()->SetPinInputEnabled(request.enable_user_input);

  if (request.error_label == chromeos::security_token_pin::ErrorLabel::kNone) {
    PinRequestWidget::Get()->UpdateState(PinRequestViewState::kNormal,
                                         GetTitle(), GetDescription());
  } else {
    PinRequestWidget::Get()->UpdateState(
        PinRequestViewState::kError,
        /*title=*/
        chromeos::security_token_pin::GenerateErrorMessage(
            request.error_label, request.attempts_left,
            request.enable_user_input),
        /*description=*/std::u16string());
  }
  return true;
}

void SecurityTokenRequestController::ClosePinUi() {
  if (!security_token_request_in_progress_) {
    return;
  }

  if (PinRequestWidget::Get()) {
    PinRequestWidget::Get()->Close(false);  // Parameter will be ignored.
  }
  on_pin_submitted_.Reset();
  on_canceled_by_user_.Reset();
  security_token_request_in_progress_ = false;
}

}  // namespace ash
