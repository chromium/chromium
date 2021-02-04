// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/account_manager_facade_factory.h"

#include "base/no_destructor.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

mojo::Remote<crosapi::mojom::AccountManager> GetAccountManagerRemote() {
  mojo::Remote<crosapi::mojom::AccountManager> remote;

  auto* lacros_chrome_service_impl = chromeos::LacrosChromeServiceImpl::Get();
  DCHECK(lacros_chrome_service_impl);
  if (!lacros_chrome_service_impl->IsAccountManagerAvailable()) {
    LOG(WARNING) << "Connected to an older version of ash. Account "
                    "consistency will not be available";
    return remote;
  }

  lacros_chrome_service_impl->BindAccountManagerReceiver(
      remote.BindNewPipeAndPassReceiver());

  return remote;
}

}  // namespace

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Multi-Login is disabled with Lacros. Always return the same instance.
  static base::NoDestructor<account_manager::AccountManagerFacadeImpl> facade(
      GetAccountManagerRemote(),
      chromeos::LacrosChromeServiceImpl::Get()->GetInterfaceVersion(
          crosapi::mojom::AccountManager::Uuid_));
  return facade.get();
}
