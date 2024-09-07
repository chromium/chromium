// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"

class AccountId;

namespace ash {

class Key;
class UserContext;

namespace quick_unlock {

enum class Purpose;

class PinStorageCryptohome {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using AvailabilityCallback =
      base::OnceCallback<void(bool, std::optional<base::Time>)>;

  // Check to see if the cryptohome implementation can store PINs.
  static void IsSupported(BoolCallback result);

  // Transforms `key` for usage in PIN. Returns nullopt if the key could not be
  // transformed.
  static std::optional<Key> TransformPinKey(
      const PinSaltStorage* pin_salt_storage,
      const AccountId& account_id,
      const Key& key);

  PinStorageCryptohome();

  PinStorageCryptohome(const PinStorageCryptohome&) = delete;
  PinStorageCryptohome& operator=(const PinStorageCryptohome&) = delete;

  ~PinStorageCryptohome();

  void IsPinSetInCryptohome(std::unique_ptr<UserContext>, BoolCallback result);
  // Sets a new PIN. If `pin_salt` is empty, `pin` will be hashed and should be
  // plain-text. If `pin_salt` contains a value, `pin` will not be hashed.
  void SetPin(std::unique_ptr<UserContext> user_context,
              const std::string& pin,
              const std::optional<std::string>& pin_salt,
              AuthOperationCallback callback);
  void RemovePin(std::unique_ptr<UserContext> user_context,
                 AuthOperationCallback callback);
  void CanAuthenticate(std::unique_ptr<UserContext> user_context,
                       Purpose purpose,
                       AvailabilityCallback result_callback);
  void TryAuthenticate(std::unique_ptr<UserContext> user_context,
                       const Key& key,
                       Purpose purpose,
                       AuthOperationCallback callback);

  void SetPinSaltStorageForTesting(
      std::unique_ptr<PinSaltStorage> pin_salt_storage);

 private:
  void OnSystemSaltObtained(const std::string& system_salt);

  // We call this after changing something in cryptohome. It reloads the
  // AuthFactorsConfiguration.
  void OnAuthFactorsEdit(AuthOperationCallback callback,
                         std::unique_ptr<UserContext> user_context,
                         std::optional<AuthenticationError> error);

  void TryAuthenticateWithAuthSession(const Key& key,
                                      AuthOperationCallback callback,
                                      bool user_exists,
                                      std::unique_ptr<UserContext> user_context,
                                      std::optional<AuthenticationError> error);

  bool salt_obtained_ = false;
  std::string system_salt_;
  std::vector<base::OnceClosure> system_salt_callbacks_;
  std::unique_ptr<PinSaltStorage> pin_salt_storage_;
  AuthFactorEditor auth_factor_editor_;
  AuthPerformer auth_performer_;

  base::WeakPtrFactory<PinStorageCryptohome> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
