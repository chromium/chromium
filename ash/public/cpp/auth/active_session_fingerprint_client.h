// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_H_
#define ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"
#include "components/account_id/account_id.h"

namespace ash {

// This enum represents the possible results of a fingerprint authentication
// scan. It's a simplified version of the FingerprintScanResult in the
// UserDataAuth.proto. We introduce this enum to avoid making ash public depend
// on a proto file, and because we only need the authentication scan results
// (not enrollment).
enum class FingerprintAuthScanResult {
  // The scan was successful and the fingerprint matched.
  kSuccess,

  // The scan was successful but the fingerprint didn't match.
  kFailed,

  // The scan failed due to too many failed attempts.
  kTooManyAttempts,

  // The scan failed due to a fatal error.
  kFatalError,

  kMaxValue = kFatalError,
};

using ActiveSessionFingerprintScanCallback =
    base::RepeatingCallback<void(const FingerprintAuthScanResult)>;

// ActiveSessionFingerprintClient handles fingerprint authentication requests
// during an active user session. It bridges the ActiveSessionAuthController
// with fingerprint storage/policy and authentication mechanisms.
class ASH_PUBLIC_EXPORT ActiveSessionFingerprintClient {
 public:
  virtual ~ActiveSessionFingerprintClient() = default;

  // Checks if the given user is permitted to use fingerprint authentication for
  // the specified purpose and if they have any enrolled fingerprints.
  virtual bool IsFingerprintAvailable(AuthRequest::Reason reason,
                                      const AccountId& account_id) = 0;

  // Prepares the biometrics daemon for authentication.
  // - `auth_ready_callback` is invoked when the daemon is ready to process
  // fingerprint scans.
  // - `on_auth_success_callback` is triggered upon successful fingerprint
  // authentication.
  // - `on_auth_failed_callback` is called when fingerprint authentication
  // fails.
  virtual void PrepareFingerprintAuth(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback auth_ready_callback,
      ActiveSessionFingerprintScanCallback on_scan_callback) = 0;

  // Returns the biometrics daemon to its normal state (e.g., when closing an
  // authentication dialog).
  virtual void TerminateFingerprintAuth(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback callback) = 0;
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_H_
