// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/security_token_pin_dialog_host_login_impl.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/check_op.h"
#include "base/functional/bind.h"

namespace ash {

SecurityTokenPinDialogHostLoginImpl::SecurityTokenPinDialogHostLoginImpl() =
    default;

SecurityTokenPinDialogHostLoginImpl::~SecurityTokenPinDialogHostLoginImpl() =
    default;

void SecurityTokenPinDialogHostLoginImpl::ShowSecurityTokenPinDialog(
    const std::string& /*caller_extension_name*/,
    chromeos::security_token_pin::CodeType code_type,
    bool enable_user_input,
    chromeos::security_token_pin::ErrorLabel error_label,
    int attempts_left,
    const std::optional<AccountId>& authenticating_user_account_id,
    SecurityTokenPinEnteredCallback pin_entered_callback,
    SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) {
  DCHECK(!enable_user_input || attempts_left);
  DCHECK_GE(attempts_left, -1);
  // There must be either no active PIN request, or the active request for which
  // the PIN has already been entered.
  DCHECK(!pin_entered_callback_);

  Reset();

  if (!authenticating_user_account_id) {
    // This class only supports requests associated with user authentication
    // attempts.
    std::move(pin_dialog_closed_callback).Run();
    return;
  }

  pin_entered_callback_ = std::move(pin_entered_callback);
  pin_dialog_closed_callback_ = std::move(pin_dialog_closed_callback);

  SecurityTokenPinRequest request;
  request.account_id = *authenticating_user_account_id;
  request.code_type = code_type;
  request.enable_user_input = enable_user_input;
  request.error_label = error_label;
  request.attempts_left = attempts_left;
  request.pin_entered_callback =
      base::BindOnce(&SecurityTokenPinDialogHostLoginImpl::OnUserInputReceived,
                     weak_ptr_factory_.GetWeakPtr());
  request.pin_ui_closed_callback =
      base::BindOnce(&SecurityTokenPinDialogHostLoginImpl::OnClosedByUser,
                     weak_ptr_factory_.GetWeakPtr());

  LoginScreen::Get()->RequestSecurityTokenPin(std::move(request));
}

void SecurityTokenPinDialogHostLoginImpl::CloseSecurityTokenPinDialog() {
  DCHECK(is_request_running());

  Reset();
  LoginScreen::Get()->ClearSecurityTokenPinRequest();
}

void SecurityTokenPinDialogHostLoginImpl::OnUserInputReceived(
    const std::string& user_input) {
  DCHECK(is_request_running());
  DCHECK(!user_input.empty());

  std::move(pin_entered_callback_).Run(user_input);
}

void SecurityTokenPinDialogHostLoginImpl::OnClosedByUser() {
  DCHECK(is_request_running());

  auto closed_callback = std::move(pin_dialog_closed_callback_);
  Reset();
  std::move(closed_callback).Run();
}

void SecurityTokenPinDialogHostLoginImpl::Reset() {
  pin_entered_callback_.Reset();
  pin_dialog_closed_callback_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace ash
