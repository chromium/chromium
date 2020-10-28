// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
class LacrosChromeServiceImpl;
}  // namespace chromeos

// Lacros specific implementation of |AccountManagerFacade| that talks to
// |chromeos::AccountManager|, residing in ash-chrome, over Mojo.
class AccountManagerFacadeLacros
    : public account_manager::AccountManagerFacade,
      public crosapi::mojom::AccountManagerObserver {
 public:
  AccountManagerFacadeLacros();
  AccountManagerFacadeLacros(const AccountManagerFacadeLacros&) = delete;
  AccountManagerFacadeLacros& operator=(const AccountManagerFacadeLacros&) =
      delete;
  ~AccountManagerFacadeLacros() override;

  // AccountManagerFacade overrides:
  bool IsInitialized() override;

  // crosapi::mojom::AccountManagerObserver overrides:
  void OnTokenUpserted(crosapi::mojom::AccountPtr account) override;
  void OnAccountRemoved(crosapi::mojom::AccountPtr account) override;

 private:
  void OnVersionCheck(uint32_t version);
  void OnReceiverReceived(
      mojo::PendingReceiver<AccountManagerObserver> receiver);
  void OnInitialized(bool is_initialized);

  bool is_initialized_ = false;
  chromeos::LacrosChromeServiceImpl* const lacros_chrome_service_impl_;
  std::unique_ptr<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>
      receiver_;

  base::WeakPtrFactory<AccountManagerFacadeLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FACADE_LACROS_H_
