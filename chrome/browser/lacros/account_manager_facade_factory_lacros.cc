// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/account_manager_facade_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/lacros/account_manager_facade_lacros.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "mojo/public/cpp/bindings/remote.h"

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Multi-Login is disabled with Lacros. Always return the same instance.
  static base::NoDestructor<AccountManagerFacadeLacros> facade([] {
    auto* lacros_chrome_service_impl = chromeos::LacrosChromeServiceImpl::Get();
    DCHECK(lacros_chrome_service_impl);
    if (!lacros_chrome_service_impl->IsAccountManagerAvailable()) {
      LOG(WARNING) << "Connected to an older version of ash. Account "
                      "consistency will not be available";
      return mojo::Remote<crosapi::mojom::AccountManager>();
    }
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    lacros_chrome_service_impl->BindAccountManagerReceiver(
        remote.BindNewPipeAndPassReceiver());
    return remote;
  }());
  return facade.get();
}
