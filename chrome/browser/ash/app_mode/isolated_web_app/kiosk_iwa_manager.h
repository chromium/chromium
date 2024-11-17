// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace ash {

class KioskIwaManager : public KioskAppManagerBase {
 public:
  static const char kIwaKioskDictionaryName[];

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the manager instance or will crash if it not yet initiazlied.
  static KioskIwaManager* Get();
  KioskIwaManager();
  KioskIwaManager(const KioskIwaManager&) = delete;
  KioskIwaManager& operator=(const KioskIwaManager&) = delete;
  ~KioskIwaManager() override;

  // KioskAppManagerBase overrides:
  KioskAppManagerBase::AppList GetApps() const override;

  // Returns app data associated with `account_id`.
  const KioskIwaData* GetApp(const AccountId& account_id) const;

  // Returns a valid account id if an IWA kiosk is configured for auto launch.
  // Returns a nullopt otherwise.
  const std::optional<AccountId>& GetAutoLaunchAccountId() const;

  // Notify this manager that a Kiosk session started with the given `app_id`.
  void OnKioskSessionStarted(const KioskAppId& app_id);

 private:
  void UpdateAppsFromPolicy() override;

  void Reset();
  void MaybeSetAutoLaunchInfo(const std::string& policy_account_id,
                              const AccountId& kiosk_app_account_id);
  void CancelCryptohomeRemovalsForCurrentApps();

  using KioskIwaDataMap = std::map<std::string, std::unique_ptr<KioskIwaData>>;
  // Clears object's internal state (apps list and autolaunch info) and returns
  // a map of previous app data.
  KioskIwaDataMap GetAppsAndReset();

  // Removes accounts associated with `previous_apps`.
  void RemoveApps(const KioskIwaDataMap& previous_apps) const;

  // Creates app data from `account` and adds it to the list of current apps.
  // Removes the matching app data from `previous_apps` that will be later
  // removed.
  void ProcessDeviceLocalAccount(const policy::DeviceLocalAccount& account,
                                 KioskIwaDataMap& previous_apps);

  // TODO(crbug.com/377878781): Make common helpers for all kiosk app managers.
  std::vector<std::unique_ptr<KioskIwaData>> isolated_web_apps_;
  std::optional<AccountId> auto_launch_id_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_
