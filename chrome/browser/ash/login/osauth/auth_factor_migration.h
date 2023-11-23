// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATION_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATION_H_

#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

namespace ash {

class UserContext;

// This interface is used by `AuthFactorMigrator` to run a migration step.
class AuthFactorMigration {
 public:
  AuthFactorMigration() = default;
  virtual ~AuthFactorMigration() = default;

  AuthFactorMigration(const AuthFactorMigration&) = delete;
  AuthFactorMigration& operator=(const AuthFactorMigration&) = delete;

  virtual void Run(std::unique_ptr<UserContext> context,
                   AuthOperationCallback callback) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_AUTH_FACTOR_MIGRATION_H_
