// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthRequest;

// ActiveSessionFingerprintClient assists ActiveSessionAuthController with
// fingerprint authentication.
class ActiveSessionFingerprintClient;

// ActiveSessionAuthController serves active session authentication requests.
// It takes care of showing and hiding the UI and the authentication process.
class ASH_PUBLIC_EXPORT ActiveSessionAuthController {
 public:
  using AuthCompletionCallback =
      base::OnceCallback<void(bool success,
                              const ash::AuthProofToken& token,
                              base::TimeDelta timeout)>;

  static ActiveSessionAuthController* Get();

  virtual ~ActiveSessionAuthController();

  // Shows a standalone authentication widget.
  // `auth_request` encapsulates surface-specific behaviors and holds the auth
  // completion callback that can be invoked via
  // `AuthStratgy::NotifyAuthSuccess` or `AuthRequest::NotifyAuthFailure`.
  // Returns whether opening the widget was successful. Will fail if another
  // widget is already opened.
  virtual bool ShowAuthDialog(std::unique_ptr<AuthRequest> auth_request) = 0;

  virtual bool IsShown() const = 0;

  // Sets the fingerprint client responsible for:
  // Managing the biometrics daemon (e.g initialize it to authentication mode).
  // Handling fingerprint authentication scan events.
  virtual void SetFingerprintClient(
      ActiveSessionFingerprintClient* fp_client) = 0;

 protected:
  ActiveSessionAuthController();
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_H_
