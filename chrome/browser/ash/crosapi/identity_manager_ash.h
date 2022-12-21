// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_IDENTITY_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_IDENTITY_MANAGER_ASH_H_

#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi identity manager interface. Lives in ash-chrome.
// Allows lacros-chrome to access properties from the identity manager that
// lives in ash, such as account names for accounts that are not yet known to
// lacros.
class IdentityManagerAsh : public mojom::IdentityManager {
 public:
  IdentityManagerAsh();
  IdentityManagerAsh(const IdentityManagerAsh&) = delete;
  IdentityManagerAsh& operator=(const IdentityManagerAsh&) = delete;
  ~IdentityManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::IdentityManager> receiver);

  // crosapi::mojom::IdentityManager:
  void GetAccountFullName(const std::string& gaia_id,
                          GetAccountFullNameCallback callback) override;
  void GetAccountImage(const std::string& gaia_id,
                       GetAccountImageCallback callback) override;
  void GetAccountEmail(const std::string& gaia_id,
                       GetAccountEmailCallback callback) override;
  void HasAccountWithPersistentError(
      const std::string& gaia_id,
      HasAccountWithPersistentErrorCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::IdentityManager> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_IDENTITY_MANAGER_ASH_H_
