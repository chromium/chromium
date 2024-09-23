// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/in_session_auth/in_session_auth_dialog_contents_view.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AuthHubConnector;

class InSessionAuthDialogControllerImpl : public InSessionAuthDialogController,
                                          public AuthAttemptConsumer {
 public:
  enum class State {
    kNotShown,
    kShowing,
    kShown,
  };

  InSessionAuthDialogControllerImpl();
  InSessionAuthDialogControllerImpl(const InSessionAuthDialogControllerImpl&) =
      delete;
  InSessionAuthDialogControllerImpl& operator=(
      const InSessionAuthDialogControllerImpl&) = delete;
  ~InSessionAuthDialogControllerImpl() override;

  // InSessionAuthDialogController overrides
  void ShowAuthDialog(
      Reason reason,
      const std::optional<std::string>& prompt,
      auth_panel::AuthCompletionCallback on_auth_complete) override;

  void SetTokenProvider(
      InSessionAuthTokenProvider* auth_token_provider) override;

  void ShowLegacyWebAuthnDialog(
      const std::string& rp_id,
      const std::string& window_id,
      WebAuthNDialogController::FinishCallback on_auth_complete) override;

  // AuthAttemptConsumer:
  void OnUserAuthAttemptRejected() override;
  void OnUserAuthAttemptConfirmed(
      AuthHubConnector* connector,
      raw_ptr<AuthFactorStatusConsumer>& out_consumer) override;
  void OnAccountNotFound() override;
  void OnUserAuthAttemptCancelled() override;
  void OnFactorAttemptFailed(AshAuthFactor factor) override;
  void OnUserAuthSuccess(AshAuthFactor factor,
                         const AuthProofToken& token) override;

 private:
  void CreateAndShowAuthPanel(
      const std::optional<std::string>& prompt,
      auth_panel::AuthCompletionCallback on_auth_complete,
      Reason reason,
      const AccountId& account_id);

  void OnAuthPanelPreferredSizeChanged();

  // Destroys the authentication dialog.
  void OnEndAuthentication();

  void NotifySuccess(const AuthProofToken& token);
  void NotifyFailure();

  // Non owning pointer, initialized and owned by
  // `ChromeBrowserMainExtraPartsAsh`.
  // `auth_token_provider_` will outlive this controller since the controller
  // is part of `ash::Shell` and will be destroyed as part of `AshShellInit`
  // before `auth_token_provider`.
  raw_ptr<InSessionAuthTokenProvider> auth_token_provider_;

  auth_panel::AuthCompletionCallback on_auth_complete_;

  State state_ = State::kNotShown;

  std::unique_ptr<views::Widget> dialog_;

  std::optional<std::string> prompt_;

  raw_ptr<InSessionAuthDialogContentsView> contents_view_ = nullptr;

  base::WeakPtrFactory<InSessionAuthDialogControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_
