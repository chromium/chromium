// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/public/auth_failure.h"

namespace ash {

AuthFailure::AuthFailure(FailureReason reason)
    : reason_(reason), error_(GoogleServiceAuthError::NONE) {
  DCHECK(reason != NETWORK_AUTH_FAILED);
}

// private
AuthFailure::AuthFailure(FailureReason reason, GoogleServiceAuthError error)
    : reason_(reason), error_(error) {}

// static
AuthFailure AuthFailure::FromNetworkAuthFailure(
    const GoogleServiceAuthError& error) {
  return AuthFailure(NETWORK_AUTH_FAILED, error);
}

const std::string AuthFailure::GetErrorString() const {
  switch (reason_) {
    case DATA_REMOVAL_FAILED:
      return "Could not destroy your old data.";
    case COULD_NOT_MOUNT_CRYPTOHOME:
      return "Could not mount cryptohome.";
    case COULD_NOT_UNMOUNT_CRYPTOHOME:
      return "Could not unmount cryptohome.";
    case COULD_NOT_MOUNT_TMPFS:
      return "Could not mount tmpfs.";
    case LOGIN_TIMED_OUT:
      return "Login timed out. Please try again.";
    case UNLOCK_FAILED:
      return "Unlock failed.";
    case NETWORK_AUTH_FAILED:
      if (error_.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
        return net::ErrorToString(error_.network_error());
      }
      return "Google authentication failed.";
    case OWNER_REQUIRED:
      return "Login is restricted to the owner's account only.";
    case ALLOWLIST_CHECK_FAILED:
      return "Login attempt blocked by allowlist.";
    case FAILED_TO_INITIALIZE_TOKEN:
      return "OAuth2 token fetch failed.";
    case MISSING_CRYPTOHOME:
      return "Cryptohome missing from disk.";
    case AUTH_DISABLED:
      return "Auth disabled for user.";
    case TPM_ERROR:
      return "Critical TPM error encountered.";
    case TPM_UPDATE_REQUIRED:
      return "TPM firmware update required.";
    case UNRECOVERABLE_CRYPTOHOME:
      return "Cryptohome is corrupted.";
    case USERNAME_HASH_FAILED:
      return "Failed to get hashed username";
    case NONE:
    case NUM_FAILURE_REASONS:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace ash
