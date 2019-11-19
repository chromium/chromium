// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/security_token_pin_dialog_host_ash_impl.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "base/logging.h"

namespace chromeos {

SecurityTokenPinDialogHostAshImpl::SecurityTokenPinDialogHostAshImpl() =
    default;

SecurityTokenPinDialogHostAshImpl::~SecurityTokenPinDialogHostAshImpl() =
    default;

void SecurityTokenPinDialogHostAshImpl::ShowSecurityTokenPinDialog(
    const std::string& /*caller_extension_name*/,
    SecurityTokenPinCodeType code_type,
    bool enable_user_input,
    SecurityTokenPinErrorLabel error_label,
    int attempts_left,
    const base::Optional<AccountId>& authenticating_user_account_id,
    SecurityTokenPinEnteredCallback pin_entered_callback,
    SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) {
  DCHECK(!enable_user_input || attempts_left);
  DCHECK_GE(attempts_left, -1);
  // There must be either no active PIN request, or the active request for which
  // the PIN has already been entered.
  DCHECK(!pin_entered_callback_);

  if (is_request_running() || !enable_user_input) {
    // Don't allow re-requesting the PIN in the same dialog after an error,
    // since the UI doesn't currently handle this in a user-friendly way.
    // TODO(crbug.com/1001288): Remove this after the proper UI error feedback
    // gets implemented in Ash.
    if (is_request_running())
      CloseSecurityTokenPinDialog();
    std::move(pin_dialog_closed_callback).Run();
    return;
  }

  Reset();

  if (!authenticating_user_account_id) {
    // This class only supports requests associated with user authentication
    // attempts.
    std::move(pin_dialog_closed_callback).Run();
    return;
  }

  pin_entered_callback_ = std::move(pin_entered_callback);
  pin_dialog_closed_callback_ = std::move(pin_dialog_closed_callback);

  ash::SecurityTokenPinRequest request;
  request.account_id = *authenticating_user_account_id;
  request.code_type = code_type;
  request.enable_user_input = enable_user_input;
  request.error_label = error_label;
  request.attempts_left = attempts_left;
  request.pin_entered_callback =
      base::BindOnce(&SecurityTokenPinDialogHostAshImpl::OnUserInputReceived,
                     weak_ptr_factory_.GetWeakPtr());
  request.pin_ui_closed_callback =
      base::BindOnce(&SecurityTokenPinDialogHostAshImpl::OnClosed,
                     weak_ptr_factory_.GetWeakPtr());

  ash::LoginScreen::Get()->RequestSecurityTokenPin(std::move(request));
}

void SecurityTokenPinDialogHostAshImpl::CloseSecurityTokenPinDialog() {
  DCHECK(is_request_running());

  Reset();
  ash::LoginScreen::Get()->ClearSecurityTokenPinRequest();
}

void SecurityTokenPinDialogHostAshImpl::OnUserInputReceived(
    const std::string& user_input) {
  DCHECK(is_request_running());
  DCHECK(!user_input.empty());

  std::move(pin_entered_callback_).Run(user_input);
}

void SecurityTokenPinDialogHostAshImpl::OnClosed() {
  DCHECK(is_request_running());

  auto closed_callback = std::move(pin_dialog_closed_callback_);
  Reset();
  std::move(closed_callback).Run();
}

void SecurityTokenPinDialogHostAshImpl::Reset() {
  pin_entered_callback_.Reset();
  pin_dialog_closed_callback_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace chromeos
