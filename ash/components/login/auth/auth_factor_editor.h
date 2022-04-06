// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_
#define ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_

#include "ash/components/login/auth/auth_callbacks.h"
#include "ash/components/login/auth/cryptohome_error.h"
#include "ash/components/login/auth/user_context.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This class provides higher level API for cryptohomed operations related to
// establishing and updating AuthFactors.
// This implementation is only compatible with AuthSession-based API.
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) AuthFactorEditor {
 public:
  AuthFactorEditor();

  AuthFactorEditor(const AuthFactorEditor&) = delete;
  AuthFactorEditor& operator=(const AuthFactorEditor&) = delete;

  ~AuthFactorEditor();

  // Invalidates any ongoing mount attempts by invalidating Weak pointers on
  // internal callbacks. Callbacks for ongoing operations will not be called
  // afterwards, but there is no guarantees about state of the factors.
  void InvalidateCurrentAttempts();

  base::WeakPtr<AuthFactorEditor> AsWeakPtr();

  // Attempts to add Kiosk-specific key to user identified by `context`.
  // Session should be authenticated.
  void AddKioskKey(std::unique_ptr<UserContext> context,
                   AuthOperationCallback callback);

  // Attempts to add Key contained in `context` to corresponding user.
  // Session should be authenticated.
  void AddContextKey(std::unique_ptr<UserContext> context,
                     AuthOperationCallback callback);

  // Attempts to replace factor labeled by Key contained in `context`
  // with key stored in ReplacementKey in the `context`.
  // Session should be authenticated.
  void ReplaceContextKey(std::unique_ptr<UserContext> context,
                         AuthOperationCallback callback);

 private:
  void HashContextKeyAndAdd(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback,
                            const std::string& system_salt);
  void OnAddCredentials(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::AddCredentialsReply> reply);

  void OnUpdateCredential(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::UpdateCredentialReply> reply);

  base::WeakPtrFactory<AuthFactorEditor> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_
