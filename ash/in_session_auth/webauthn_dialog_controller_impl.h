// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
#define ASH_IN_SESSION_AUTH_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/in_session_auth/in_session_auth_dialog.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_tracker.h"

class AccountId;

namespace aura {
class Window;
}

namespace ash {

class InSessionAuthDialogClient;
class WebAuthnRequestRegistrarImpl;

// WebAuthNDialogControllerImpl persists as long as UI is running.
class WebAuthNDialogControllerImpl : public WebAuthNDialogController {
 public:
  WebAuthNDialogControllerImpl();
  WebAuthNDialogControllerImpl(const WebAuthNDialogControllerImpl&) = delete;
  WebAuthNDialogControllerImpl& operator=(const WebAuthNDialogControllerImpl&) =
      delete;
  ~WebAuthNDialogControllerImpl() override;

  // WebAuthNDialogController overrides
  void SetClient(InSessionAuthDialogClient* client) override;
  void ShowAuthenticationDialog(aura::Window* source_window,
                                const std::string& origin_name,
                                FinishCallback finish_callback) override;
  void DestroyAuthenticationDialog() override;
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      OnAuthenticateCallback callback) override;
  void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, FingerprintState)> callback) override;
  void OpenInSessionAuthHelpPage() override;
  void Cancel() override;
  void CheckAvailability(FinishCallback on_availability_checked) const override;

 private:
  bool IsFingerprintAvailable(const AccountId& account_id);
  void CheckAuthFactorAvailability(const AccountId& account_id,
                                   const std::string& origin_name,
                                   uint32_t auth_methods,
                                   aura::Window* source_window);
  void OnStartFingerprintAuthSession(AccountId account_id,
                                     uint32_t auth_methods,
                                     aura::Window* source_window,
                                     const std::string& origin_name,
                                     bool success);
  void OnPinCanAuthenticate(uint32_t auth_methods,
                            aura::Window* source_window,
                            const std::string& origin_name,
                            bool pin_auth_available);

  // Callback to execute when auth on ChromeOS side completes.
  void OnAuthenticateComplete(OnAuthenticateCallback callback, bool success);

  void OnFingerprintAuthComplete(
      base::OnceCallback<void(bool, FingerprintState)> views_callback,
      bool success,
      FingerprintState fingerprint_state);

  // Process final cleanup tasks.
  void ProcessFinalCleanups();

  // Called when auth succeeds to close the dialog and report success.
  void OnAuthSuccess();

  raw_ptr<InSessionAuthDialogClient> client_ = nullptr;

  // Callback to provide result of the entire authentication flow to
  // UserAuthenticationServiceProvider.
  FinishCallback finish_callback_;

  std::unique_ptr<InSessionAuthDialog> dialog_;

  aura::WindowTracker source_window_tracker_;

  std::unique_ptr<WebAuthnRequestRegistrarImpl> webauthn_request_registrar_;

  base::WeakPtrFactory<WebAuthNDialogControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
