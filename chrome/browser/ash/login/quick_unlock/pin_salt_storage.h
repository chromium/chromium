// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_

#include <string>

class AccountId;

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
  PinSaltStorageImpl();
  PinSaltStorageImpl(const PinSaltStorageImpl&) = delete;
  PinSaltStorageImpl& operator=(const PinSaltStorageImpl&) = delete;
  ~PinSaltStorageImpl() override;

  // PinSaltStorage overrides:
  std::string GetSalt(const AccountId& account_id) const override;
  void WriteSalt(const AccountId& account_id, const std::string& salt) override;
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_
