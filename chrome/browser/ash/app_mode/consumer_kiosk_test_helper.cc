// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_

#include "chrome/browser/ash/app_mode/consumer_kiosk_test_helper.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/device_local_account_type.h"

namespace ash {

namespace {

// Domain used for Kiosk app account IDs.
constexpr char kKioskAppAccountDomain[] = "kiosk-apps";

std::string GenerateKioskAppAccountId(const std::string& chrome_app_id) {
  return chrome_app_id + '@' + kKioskAppAccountDomain;
}

}  // namespace

void SetConsumerKioskAutoLaunchChromeAppForTesting(
    KioskChromeAppManager& manager,
    OwnerSettingsServiceAsh& service,
    const std::string& chrome_app_id) {
  // Clean first so the change callbacks on `KioskChromeAppManager` are called.
  if (!manager.GetAutoLaunchApp().empty()) {
    service.SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                      std::string());
  }

  service.SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                    chrome_app_id.empty()
                        ? std::string()
                        : GenerateKioskAppAccountId(chrome_app_id));
  service.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
}

void AddConsumerKioskChromeAppForTesting(OwnerSettingsServiceAsh& service,
                                         const std::string& chrome_app_id) {
  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());

  // Don't insert the app if it's already in the list.
  for (const auto& device_local_account : device_local_accounts) {
    if (device_local_account.type ==
            policy::DeviceLocalAccountType::kKioskApp &&
        device_local_account.kiosk_app_id == chrome_app_id) {
      return;
    }
  }

  // Add the new account.
  device_local_accounts.emplace_back(
      policy::DeviceLocalAccountType::kKioskApp,
      policy::DeviceLocalAccount::EphemeralMode::kUnset,
      GenerateKioskAppAccountId(chrome_app_id), chrome_app_id,
      /*update_url=*/std::string());

  policy::SetDeviceLocalAccountsForTesting(&service, device_local_accounts);
}

void RemoveConsumerKioskChromeAppForTesting(KioskChromeAppManager& manager,
                                            OwnerSettingsServiceAsh& service,
                                            const std::string& chrome_app_id) {
  // Resets auto launch app if it is the removed app.
  if (manager.GetAutoLaunchApp() == chrome_app_id) {
    SetConsumerKioskAutoLaunchChromeAppForTesting(manager, service,
                                                  std::string());
  }

  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  if (device_local_accounts.empty()) {
    return;
  }

  // TODO(crbug.com/358022471) Stop using multiple Kiosk apps with the same ID.
  // Remove the first device local account with the given `chrome_app_id`.
  auto it = base::ranges::find_if(
      device_local_accounts, [chrome_app_id](const auto& account) {
        return account.type == policy::DeviceLocalAccountType::kKioskApp &&
               account.kiosk_app_id == chrome_app_id;
      });
  if (it != std::end(device_local_accounts)) {
    device_local_accounts.erase(it);
  }

  policy::SetDeviceLocalAccountsForTesting(&service, device_local_accounts);
}

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CONSUMER_KIOSK_TEST_HELPER_H_
