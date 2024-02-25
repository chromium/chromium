// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_PIN_DIALOG_HOST_LOGIN_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_PIN_DIALOG_HOST_LOGIN_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/certificate_provider/security_token_pin_dialog_host.h"

namespace ash {

// The Ash Login/Lock screen implementation of the security token PIN dialog
// host. It displays the PIN request embedded into the user pod.
class SecurityTokenPinDialogHostLoginImpl final
    : public chromeos::SecurityTokenPinDialogHost {
 public:
  SecurityTokenPinDialogHostLoginImpl();
  SecurityTokenPinDialogHostLoginImpl(
      const SecurityTokenPinDialogHostLoginImpl&) = delete;
  SecurityTokenPinDialogHostLoginImpl& operator=(
      const SecurityTokenPinDialogHostLoginImpl&) = delete;
  ~SecurityTokenPinDialogHostLoginImpl() override;

  // SecurityTokenPinDialogHost:
  void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      chromeos::security_token_pin::CodeType code_type,
      bool enable_user_input,
      chromeos::security_token_pin::ErrorLabel error_label,
      int attempts_left,
      const std::optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) override;
  void CloseSecurityTokenPinDialog() override;

 private:
  bool is_request_running() const {
    return !pin_dialog_closed_callback_.is_null();
  }

  // Called when the PIN entered by the user is received from the Ash Login/Lock
  // Screen UI.
  void OnUserInputReceived(const std::string& user_input);
  // Called when the PIN UI gets closed.
  void OnClosedByUser();

  // Resets the internal state and weak pointers associated with the previously
  // started requests.
  void Reset();

  // The callback to run when the user submits a non-empty input to the security
  // token PIN dialog.
  // Is non-empty iff the dialog is active and the input wasn't sent yet.
  SecurityTokenPinEnteredCallback pin_entered_callback_;
  // The callback to run when the security token PIN dialog gets closed.
  // Is non-empty iff the dialog is active.
  SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback_;

  base::WeakPtrFactory<SecurityTokenPinDialogHostLoginImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_PIN_DIALOG_HOST_LOGIN_IMPL_H_
