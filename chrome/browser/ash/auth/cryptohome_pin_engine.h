// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUTH_CRYPTOHOME_PIN_ENGINE_H_
#define CHROME_BROWSER_ASH_AUTH_CRYPTOHOME_PIN_ENGINE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"

namespace ash {

class UserContext;

namespace legacy {
// Handles Pin related authentication operations and is the source of truth
// for the availability of Pin authentication.
class CryptohomePinEngine {
 public:
  enum class Purpose { kAny, kUnlock, kWebAuthn };

  explicit CryptohomePinEngine(ash::AuthPerformer* auth_performer);
  CryptohomePinEngine(const CryptohomePinEngine&) = delete;
  CryptohomePinEngine& operator=(const CryptohomePinEngine&) = delete;
  virtual ~CryptohomePinEngine();

  using IsPinAuthAvailableCallback =
      base::OnceCallback<void(bool, std::unique_ptr<UserContext>)>;

  // Checks if pin setup should be skipped due to policy.
  bool ShouldSkipSetupBecauseOfPolicy(const AccountId& account_id) const;

  // Checks if pin is disabled by policy for the given `account_id` and
  // `purpose`.
  std::optional<bool> IsCryptohomePinDisabledByPolicy(
      const AccountId& account_id,
      CryptohomePinEngine::Purpose purpose) const;

  // Checks the availability of Pin authentication, based on things like
  // policy configuration and whether or not the auth factor is set up.
  void IsPinAuthAvailable(Purpose purpose,
                          std::unique_ptr<UserContext> user_context,
                          IsPinAuthAvailableCallback callback);

  // The `user_context` parameter must have an associated `authsession_id`,
  // acquired from a call to `AuthPerformer::StartAuthSession`.
  void Authenticate(const cryptohome::RawPin& pin,
                    std::unique_ptr<UserContext> user_context,
                    AuthOperationCallback callback);

 private:
  // Checks for Pin factor availability and lockout status
  void CheckCryptohomePinFactor(std::unique_ptr<UserContext> user_context,
                                IsPinAuthAvailableCallback callback);

  void OnGetAuthFactorsConfiguration(IsPinAuthAvailableCallback callback,
                                     std::unique_ptr<UserContext> user_context,
                                     std::optional<AuthenticationError> error);

  // Non owning pointer
  const raw_ptr<ash::AuthPerformer> auth_performer_;

  ash::AuthFactorEditor auth_factor_editor_;

  base::WeakPtrFactory<CryptohomePinEngine> weak_factory_{this};
};

}  // namespace legacy

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUTH_CRYPTOHOME_PIN_ENGINE_H_
