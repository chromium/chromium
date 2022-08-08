// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_ERROR_H_
#define ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_ERROR_H_

#include "ash/components/login/auth/public/auth_failure.h"
#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace ash {

// Struct that wraps implementation details of cryptohomed error reporting.
struct COMPONENT_EXPORT(ASH_LOGIN_AUTH) CryptohomeError {
  explicit CryptohomeError(user_data_auth::CryptohomeErrorCode error_code);
  ~CryptohomeError();

  // Original error reported by cryptohome.
  user_data_auth::CryptohomeErrorCode error_code;

  // Mapping of the `error_code` to auth flow failure reason.
  AuthFailure::FailureReason failure_reason = AuthFailure::NONE;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CRYPTOHOME_ERROR_H_
