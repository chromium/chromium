// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/login/auth/user_context.h"

class AccountId;

namespace chromeos {

class UserContext;

namespace quick_unlock {

class PinStorageCryptohome {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  // Check to see if the cryptohome implementation can store PINs.
  static void IsSupported(BoolCallback result);

  // Transforms |key| for usage in PIN. Returns nullopt if the key could not be
  // transformed.
  static base::Optional<Key> TransformKey(const AccountId& account_id,
                                          const Key& key);

  PinStorageCryptohome();
  ~PinStorageCryptohome();

  void IsPinSetInCryptohome(const AccountId& account_id,
                            BoolCallback result) const;
  // Sets a new PIN. If |pin_salt| is empty, |pin| will be hashed and should be
  // plain-text. If |pin_salt| contains a value, |pin| will not be hashed.
  void SetPin(const UserContext& user_context,
              const std::string& pin,
              const base::Optional<std::string>& pin_salt,
              BoolCallback did_set);
  void RemovePin(const UserContext& user_context, BoolCallback did_remove);
  void CanAuthenticate(const AccountId& account_id, BoolCallback result) const;
  void TryAuthenticate(const AccountId& account_id,
                       const Key& key,
                       BoolCallback result);

 private:
  void OnSystemSaltObtained(const std::string& system_salt);

  bool salt_obtained_ = false;
  std::string system_salt_;
  std::vector<base::OnceClosure> system_salt_callbacks_;

  base::WeakPtrFactory<PinStorageCryptohome> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PinStorageCryptohome);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_CRYPTOHOME_H_
