// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ash {

class Key;
class UserContext;

namespace quick_unlock {
enum class Purpose;

class PinStorageCryptohome {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  // Check to see if the cryptohome implementation can store PINs.
  static void IsSupported(BoolCallback result);

  // Transforms `key` for usage in PIN. Returns nullopt if the key could not be
  // transformed.
  static absl::optional<Key> TransformPinKey(
      const PinSaltStorage* pin_salt_storage,
      const AccountId& account_id,
      const Key& key);

  PinStorageCryptohome();

  PinStorageCryptohome(const PinStorageCryptohome&) = delete;
  PinStorageCryptohome& operator=(const PinStorageCryptohome&) = delete;

  ~PinStorageCryptohome();

  void IsPinSetInCryptohome(const AccountId& account_id,
                            BoolCallback result) const;
  // Sets a new PIN. If `pin_salt` is empty, `pin` will be hashed and should be
  // plain-text. If `pin_salt` contains a value, `pin` will not be hashed.
  void SetPin(const UserContext& user_context,
              const std::string& pin,
              const absl::optional<std::string>& pin_salt,
              BoolCallback did_set);
  void RemovePin(const UserContext& user_context, BoolCallback did_remove);
  void CanAuthenticate(const AccountId& account_id,
                       Purpose purpose,
                       BoolCallback result) const;
  void TryAuthenticate(const AccountId& account_id,
                       const Key& key,
                       Purpose purpose,
                       BoolCallback result);

  void SetPinSaltStorageForTesting(
      std::unique_ptr<PinSaltStorage> pin_salt_storage);

 private:
  void OnSystemSaltObtained(const std::string& system_salt);

  bool salt_obtained_ = false;
  std::string system_salt_;
  std::vector<base::OnceClosure> system_salt_callbacks_;
  std::unique_ptr<PinSaltStorage> pin_salt_storage_;

  base::WeakPtrFactory<PinStorageCryptohome> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace quick_unlock {
using ::ash::quick_unlock::PinStorageCryptohome;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
