// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_ownership_waiter_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

void DeviceOwnershipWaiterImpl::WaitForOwnershipFetched(
    base::OnceClosure callback,
    bool launching_at_login_screen) {
  if (launching_at_login_screen ||
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      profiles::IsDemoSession() || !base::SysInfo::IsRunningOnChromeOS()) {
    std::move(callback).Run();
    return;
  }

  // We assume that there are no kiosk sessions in consumer setups, for more
  // information see docs of this method.
  CHECK(policy::ManagementServiceFactory::GetForPlatform()->IsManaged() ||
        !user_manager::UserManager::Get()->IsLoggedInAsKioskApp());

  user_manager::UserManager::Get()->GetOwnerAccountIdAsync(
      base::IgnoreArgs<const AccountId&>(std::move(callback)));
}

}  // namespace crosapi
