// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CLIENT_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/callback_forward.h"
#include "components/account_id/account_id.h"

namespace aura {
class Window;
}

namespace ash {

// An interface that allows Ash to trigger authentication steps that ChromeOS
// is responsible for.
class ASH_PUBLIC_EXPORT InSessionAuthDialogClient {
 public:
  // Starts a cryptohome auth session that spans the life of the dialog.
  virtual void StartAuthSession(base::OnceCallback<void(bool)> callback) = 0;

  // Ends the cryptohome auth session when the dialog is destroyed.
  virtual void InvalidateAuthSession() = 0;

  // Attempt to authenticate the current session user with a password or PIN.
  // |password|: The submitted password.
  // |authenticated_by_pin|: True if we are authenticating by PIN..
  // |callback|: the callback to run on auth complete.
  virtual void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) = 0;

  // Check whether fingerprint auth is available for |account_id|.
  virtual bool IsFingerprintAuthAvailable(const AccountId& account_id) = 0;

  // Switch biometrics daemon to auth mode.
  virtual void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  // Switch biometrics daemon to normal mode. Used when closing the dialog.
  virtual void EndFingerprintAuthSession(base::OnceClosure callback) = 0;

  // Check whether PIN auth is available for |account_id|.
  virtual void CheckPinAuthAvailability(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  virtual void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, FingerprintState)> callback) = 0;

  // Open a help article in a new window and return the window.
  virtual aura::Window* OpenInSessionAuthHelpPage() const = 0;

 protected:
  virtual ~InSessionAuthDialogClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CLIENT_H_
