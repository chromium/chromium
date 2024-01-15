// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "components/account_id/account_id.h"

namespace chromeos {

// The interface that allows showing a PIN dialog requested by a certificate
// provider extension (see the chrome.certificateProvider API).
//
// Only one dialog is supported at a time by a single host instance.
class SecurityTokenPinDialogHost {
 public:
  using SecurityTokenPinEnteredCallback =
      base::OnceCallback<void(const std::string& user_input)>;
  using SecurityTokenPinDialogClosedCallback = base::OnceClosure;

  // Note that this does NOT execute the callback.
  virtual ~SecurityTokenPinDialogHost() = default;

  // Shows the PIN dialog, or updates the existing one if it's already shown.
  //
  // Note that the caller is still responsible for closing the opened dialog, by
  // calling CloseSecurityTokenPinDialog(), after the |callback| got executed
  // with a non-empty |user_input|.
  //
  // Note also that when the existing dialog is updated, its old callbacks will
  // NOT be called at all.
  //
  // |caller_extension_name| - name of the extension that requested this dialog.
  // |code_type| - type of the code requested from the user.
  // |enable_user_input| - when false, the UI will disable the controls that
  //     allow user to enter the PIN/PUK. MUST be |false| when |attempts_left|
  //     is zero.
  // |error_label| - optionally, specifies the error that the UI should display
  //     (note that a non-empty error does NOT disable the user input per se).
  // |attempts_left| - when non-negative, the UI should indicate this number to
  //     the user; otherwise must be equal to -1.
  // |authenticating_user_account_id| - when set, is the ID of the user whose
  //     authentication triggered this PIN request.
  // |pin_entered_callback| - called when the user submits the input.
  // |pin_dialog_closed_callback| - called when the dialog is closed (either by
  //     the user or programmatically; it's optional whether to call it after
  //     CloseSecurityTokenPinDialog()).
  virtual void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      security_token_pin::CodeType code_type,
      bool enable_user_input,
      security_token_pin::ErrorLabel error_label,
      int attempts_left,
      const std::optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) = 0;

  // Closes the currently shown PIN dialog, if there's any. The implementation
  // is NOT required to run |pin_dialog_closed_callback| after the closing.
  virtual void CloseSecurityTokenPinDialog() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_H_
