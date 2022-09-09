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
  PinSaltStorage();

  PinSaltStorage(const PinSaltStorage&) = delete;
  PinSaltStorage& operator=(const PinSaltStorage&) = delete;

  virtual ~PinSaltStorage();

  virtual std::string GetSalt(const AccountId& account_id) const;
  virtual void WriteSalt(const AccountId& account_id, const std::string& salt);
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_SALT_STORAGE_H_
