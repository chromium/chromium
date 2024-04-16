// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"

#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

namespace {

ash::AccountManagerFactory* GetAccountManagerFactory() {
  return g_browser_process->platform_part()->GetAccountManagerFactory();
}

crosapi::AccountManagerMojoService* GetAccountManagerMojoService(
    const std::string& profile_path) {
  crosapi::AccountManagerMojoService* account_manager_mojo_service =
      GetAccountManagerFactory()->GetAccountManagerMojoService(profile_path);
  DCHECK(account_manager_mojo_service);

  return account_manager_mojo_service;
}

}  // namespace

account_manager::AccountManagerFacade* GetAccountManagerFacade(
    const std::string& profile_path) {
  // Map from |profile_path| to AccountManagerFacade.
  static base::NoDestructor<std::map<
      std::string, std::unique_ptr<account_manager::AccountManagerFacadeImpl>>>
      account_manager_facade_map;

  auto it = account_manager_facade_map->find(profile_path);
  if (it == account_manager_facade_map->end()) {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    GetAccountManagerMojoService(profile_path)
        ->BindReceiver(remote.BindNewPipeAndPassReceiver());

    // This is set to a sentinel value which will pass all minimum version
    // checks.
    // Calls within Ash are in the same process and don't need to check version
    // compatibility with itself.
    constexpr uint32_t remote_version = std::numeric_limits<uint32_t>::max();
    // TODO(crbug.com/40800999): to avoid incorrect usage, pass a nullptr
    // `AccountManager` when this is not running in a test.
    account_manager::AccountManager* account_manager_for_tests =
        GetAccountManagerFactory()->GetAccountManager(profile_path);
    auto account_manager_facade =
        std::make_unique<account_manager::AccountManagerFacadeImpl>(
            std::move(remote), remote_version,
            account_manager_for_tests->GetWeakPtr());
    it = account_manager_facade_map
             ->emplace(profile_path, std::move(account_manager_facade))
             .first;
  }

  return it->second.get();
}
