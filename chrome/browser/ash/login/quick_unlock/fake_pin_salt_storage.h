// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FAKE_PIN_SALT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FAKE_PIN_SALT_STORAGE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace quick_unlock {

// FakePinSaltStorage is an in-memory storage for salts used by tests.
class FakePinSaltStorage : public PinSaltStorage {
 public:
  FakePinSaltStorage();

  FakePinSaltStorage(const FakePinSaltStorage&) = delete;
  FakePinSaltStorage& operator=(const FakePinSaltStorage&) = delete;

  ~FakePinSaltStorage() override;

  std::string GetSalt(const AccountId& account_id) const override;
  void WriteSalt(const AccountId& account_id, const std::string& salt) override;

 private:
  base::flat_map<AccountId, std::string> salt_storage_;
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FAKE_PIN_SALT_STORAGE_H_
