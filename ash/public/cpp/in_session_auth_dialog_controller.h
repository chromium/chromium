// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// InSessionAuthDialogController manages the in session auth dialog.
class ASH_PUBLIC_EXPORT InSessionAuthDialogController {
 public:
  enum Reason {
    kAccessPasswordManager,
    kAccessAuthenticationSettings,
    kAccessMultideviceSettings,
  };

  // Returns the singleton instance.
  static InSessionAuthDialogController* Get();

  // Summons a native UI dialog that authenticates the user, providing a
  // token, timeout and status in return.
  // `reason`: Indicates security context.
  virtual void ShowAuthDialog(
      Reason reason,
      const std::optional<std::string>& prompt,
      auth_panel::AuthCompletionCallback on_auth_complete) = 0;

  // Summons the WebAuthn UI dialog that authenticates the user.
  virtual void ShowLegacyWebAuthnDialog(
      const std::string& rp_id,
      const std::string& window_id,
      WebAuthNDialogController::FinishCallback on_auth_complete) = 0;

  // Must be called with a non null auth_token_provider prior to calling
  // `ShowAuthDialog`.
  // Injects a specific implementation of `InSessionAuthTokenProvider`
  // for generating an `AuthToken` after successful authentication.
  virtual void SetTokenProvider(
      InSessionAuthTokenProvider* auth_token_provider) = 0;

 protected:
  InSessionAuthDialogController();
  virtual ~InSessionAuthDialogController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
