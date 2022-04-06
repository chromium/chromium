// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_
#define ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_

#include <memory>

#include "ash/components/login/auth/auth_callbacks.h"
#include "ash/components/login/auth/cryptohome_error.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class UserContext;

// This class provides higher level API for cryptohomed operations related to
// AuthSession: It starts new auth sessions, can authenticate auth session
// using various factors, extend session lifetime, close session.
// This implementation is only compatible with AuthSession-based API.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) AuthPerformer {
 public:
  AuthPerformer();

  AuthPerformer(const AuthPerformer&) = delete;
  AuthPerformer& operator=(const AuthPerformer&) = delete;

  ~AuthPerformer();

  using StartSessionCallback =
      base::OnceCallback<void(bool /* user_exists */,
                              std::unique_ptr<UserContext>,
                              absl::optional<CryptohomeError>)>;

  // Invalidates any ongoing mount attempts by invalidating Weak pointers on
  // internal callbacks. Callbacks for ongoing operations will not be called
  // afterwards, but there is no guarantees about state of the session.
  void InvalidateCurrentAttempts();

  // Starts new AuthSession using identity passed in `context`,
  // fills information about supported (and configured if user exists) keys.
  // `Context` should not have associated auth session.
  // Does not authenticate new session.
  void StartAuthSession(std::unique_ptr<UserContext> context,
                        bool ephemeral,
                        StartSessionCallback callback);

  // Attempts to authenticate session using Key in `context`.
  // If key is a plain text, it is assumed that it is a password key,
  // so it will be hashed accordingly, and key label would be backfilled
  // if not specified explicitly.
  // In all other cases it is assumed that all fields are filled correctly.
  // Session will become authenticated upon success.
  void AuthenticateUsingKey(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback);

  // Attempts to authenticate Kiosk session using specific key based on
  // identity.
  // Session will become authenticated upon success.
  void AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                           AuthOperationCallback callback);

 private:
  void OnServiceRunning(std::unique_ptr<UserContext> context,
                        bool ephemeral,
                        StartSessionCallback callback,
                        bool service_is_running);
  void OnStartAuthSession(
      std::unique_ptr<UserContext> context,
      StartSessionCallback callback,
      absl::optional<user_data_auth::StartAuthSessionReply> reply);

  void HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                              AuthOperationCallback callback,
                              const std::string& system_salt);

  void OnAuthenticateAuthSession(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply);

  base::WeakPtrFactory<AuthPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_
