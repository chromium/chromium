// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WEBAUTHN_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_WEBAUTHN_DIALOG_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"

namespace aura {
class Window;
}

namespace ash {

// WebAuthNDialogController manages the webauthn auth dialog.
class ASH_PUBLIC_EXPORT WebAuthNDialogController {
 public:
  // Callback for authentication checks. |success| true/false if auth
  // succeeded/failed, and |can_use_pin| indicates whether PIN can still be used
  // (not locked out) after the previous authentication.
  using OnAuthenticateCallback =
      base::OnceCallback<void(bool success, bool can_use_pin)>;
  // Callback for overall authentication flow result.
  using FinishCallback = base::OnceCallback<void(bool success)>;

  // Return the singleton instance.
  static WebAuthNDialogController* Get();

  // Sets the client that will handle authentication.
  virtual void SetClient(InSessionAuthDialogClient* client) = 0;

  // Displays the authentication dialog for the website/app name in |app_id|.
  virtual void ShowAuthenticationDialog(aura::Window* source_window,
                                        const std::string& origin_name,
                                        FinishCallback finish_callback) = 0;

  // Destroys the authentication dialog.
  virtual void DestroyAuthenticationDialog() = 0;

  // Takes a password or PIN and sends it to InSessionAuthDialogClient to
  // authenticate. The InSessionAuthDialogClient should already know the current
  // session's active user, so the user account is not provided here.
  virtual void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      OnAuthenticateCallback callback) = 0;

  // Requests ChromeOS to report fingerprint scan result through |callback|.
  virtual void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, FingerprintState)> callback) = 0;

  // Opens a help article in Chrome.
  virtual void OpenInSessionAuthHelpPage() = 0;

  // Cancels all operations and destroys the dialog.
  virtual void Cancel() = 0;

  // Checks whether there's at least one authentication method.
  virtual void CheckAvailability(
      FinishCallback on_availability_checked) const = 0;

 protected:
  WebAuthNDialogController();
  virtual ~WebAuthNDialogController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WEBAUTHN_DIALOG_CONTROLLER_H_
