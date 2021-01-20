// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/account_manager_facade_factory.h"

#include <map>
#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade_impl.h"

namespace {

crosapi::AccountManagerAsh* GetAccountManagerAsh(
    const std::string& profile_path) {
  crosapi::AccountManagerAsh* account_manager_ash =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManagerAsh(profile_path);
  DCHECK(account_manager_ash);

  return account_manager_ash;
}

}  // namespace

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Map from |profile_path| to AccountManagerFacade.
  static base::NoDestructor<
      std::map<std::string, std::unique_ptr<AccountManagerFacadeImpl>>>
      account_manager_facade_map;

  auto it = account_manager_facade_map->find(profile_path);
  if (it == account_manager_facade_map->end()) {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    GetAccountManagerAsh(profile_path)
        ->BindReceiver(remote.BindNewPipeAndPassReceiver());
    it = account_manager_facade_map
             ->emplace(profile_path, std::make_unique<AccountManagerFacadeImpl>(
                                         std::move(remote)))
             .first;
  }

  return it->second.get();
}
