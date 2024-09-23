// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_
#define ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_

#include <memory>
#include <optional>

#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Textfield;
}

namespace ash {

class AuthenticationError;

// To be used for in-session authentication. Currently, only password
// is supported, however, there are plans to enrich this dialog to eventually
// support all configured forms of authentication on the system.
class AuthenticationDialog : public views::DialogDelegateView {
 public:
  class TestApi {
   public:
    explicit TestApi(AuthenticationDialog* dialog) : dialog_(dialog) {}

    views::Textfield* GetPasswordTextfield() {
      return dialog_->password_field_;
    }

   private:
    raw_ptr<AuthenticationDialog, AcrossTasksDanglingUntriaged> const dialog_;
  };

  // |on_auth_complete| is called when the user has been authenticated
  // or when the dialog has been aborted
  explicit AuthenticationDialog(
      auth_panel::AuthCompletionCallback on_auth_complete,
      InSessionAuthTokenProvider* auth_token_provider,
      std::unique_ptr<AuthPerformer> auth_performer,
      const AccountId& account_id);

  ~AuthenticationDialog() override;

  // Creates and displays a new instance of a widget that hosts the
  // AuthenticationDialog.
  void Show();

 private:
  // Called post widget initialization. For now, this configures the Ok button
  // with custom behavior needed to handle retry of password entry. Also focuses
  // the text input field.
  void Init();

  // Calls `on_auth_complere_` with `success` == true if
  // authentication was successful, and `success` == false if the dialog was
  // aborted.
  void NotifyResult(bool success,
                    const AuthProofToken& token,
                    base::TimeDelta timeout);

  // Modifies the Ok button to display the proper string and registers
  // `ValidateAuthFactor` as a callback.
  void ConfigureOkButton();

  // Disables the use of the OK and Cancel buttons, makes password text field
  // read-only.
  void SetUIDisabled(bool is_disabled);

  // Registered as a callback to the Ok button. Disables UI, and validates the
  // auth factor.
  void ValidateAuthFactor();

  // Passed as a callback to `AuthPerformer::AuthenticateWithPassword`, notifies
  // the dialog of authentication success or failure, in case of failure we
  // modify the UI appropriately, in case of success we close the dialog.
  void OnAuthFactorValidityChecked(
      std::unique_ptr<UserContext> user_context,
      std::optional<AuthenticationError> cryptohome_error);

  // Show an auth error in the UI and mark the password field as invalid.
  void ShowAuthError();

  // Registered as a callback to the Cancel and Close buttons. Calls
  // `NotifyResult` with `success` == false.
  void CancelAuthAttempt();

  // Configures the different subviews such as the password textfield and the
  // error message label.
  void ConfigureChildViews();

  // Passed as a callback to `AuthPerformer::StartAuthSession` in
  // `OnAuthFactorValidityChecked` when trying to validate the password
  // and discovering that the auth session is no longer active
  void OnAuthSessionInvalid(bool user_exists,
                            std::unique_ptr<UserContext> user_context,
                            std::optional<AuthenticationError> auth_error);

  // Passed as a callback to `AuthPerformer::StartAuthSession`. Saves the
  // password key label to pass it later to authentication attempts and handles
  // errors from cryptohome
  void OnAuthSessionStarted(bool user_exists,
                            std::unique_ptr<UserContext> user_context,
                            std::optional<AuthenticationError> auth_error);

  raw_ptr<views::Textfield> password_field_;
  raw_ptr<views::Label> invalid_password_label_;

  // See implementation of `CancelAuthAttempt` for details.
  bool is_closing_ = false;

  auth_panel::AuthCompletionCallback on_auth_complete_;

  // Called when user submits an auth factor to check its validity
  std::unique_ptr<AuthPerformer> auth_performer_;

  // Non owning pointer, initialized and owned by
  // `ChromeBrowserMainExtraPartsAsh`.
  // `auth_token_provider_` will outlive this dialog since it will
  // be destroyed after `AshShellInit`, which owns the aura
  // window hierarchy.
  raw_ptr<InSessionAuthTokenProvider> auth_token_provider_;

  std::unique_ptr<UserContext> user_context_;

  base::WeakPtrFactory<AuthenticationDialog> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_
