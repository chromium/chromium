// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_

#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
class AccountManager;
}  // namespace chromeos

namespace crosapi {

// Implements the |crosapi::mojom::AccountManager| interface in ash-chrome.
// It enables lacros-chrome to interact with accounts stored in the Chrome OS
// Account Manager.
class AccountManagerAsh : public mojom::AccountManager {
 public:
  AccountManagerAsh(chromeos::AccountManager* account_manager,
                    mojo::PendingReceiver<mojom::AccountManager> receiver);
  AccountManagerAsh(const AccountManagerAsh&) = delete;
  AccountManagerAsh& operator=(const AccountManagerAsh&) = delete;
  ~AccountManagerAsh() override;

  // crosapi::mojom::AccountManager:
  void IsInitialized(IsInitializedCallback callback) override;

 private:
  chromeos::AccountManager* const account_manager_;
  mojo::Receiver<mojom::AccountManager> receiver_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_
