// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_

#include <string>

#include "base/memory/raw_ref.h"

class AccountId;
class PrefService;

namespace ash {
namespace quick_unlock {

// PinSaltStorage is in charge of writing and getting the salt for accounts.
class PinSaltStorage {
 public:
  virtual ~PinSaltStorage() = default;
  virtual std::string GetSalt(const AccountId& account_id) const = 0;
  virtual void WriteSalt(const AccountId& account_id,
                         const std::string& salt) = 0;
};

class PinSaltStorageImpl : public PinSaltStorage {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit PinSaltStorageImpl(PrefService* local_state);
  PinSaltStorageImpl(const PinSaltStorageImpl&) = delete;
  PinSaltStorageImpl& operator=(const PinSaltStorageImpl&) = delete;
  ~PinSaltStorageImpl() override;

  // PinSaltStorage overrides:
  std::string GetSalt(const AccountId& account_id) const override;
  void WriteSalt(const AccountId& account_id, const std::string& salt) override;

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_
