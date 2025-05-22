// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

class KioskArcvmAppData;

// Keeps track of Android apps that are to be launched in kiosk mode.
// For removed apps deletes appropriate cryptohome. The information about
// kiosk apps are received from CrosSettings. For each app, the system
// creates a user in whose context the app then runs.
class KioskArcvmAppManager : public KioskAppManagerBase {
 public:
  static KioskArcvmAppManager* Get();
  // local_state:  Stores user defined app data such as name, icon, etc.
  // It should outlive KioskArcvmAppManager.
  explicit KioskArcvmAppManager(PrefService* local_state);
  KioskArcvmAppManager();
  KioskArcvmAppManager(const KioskArcvmAppManager&) = delete;
  KioskArcvmAppManager& operator=(const KioskArcvmAppManager&) = delete;
  ~KioskArcvmAppManager() override;

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns app that should be started for given account id.
  // Returns nullptr if no apps are defined for the given `account_id`.
  const KioskArcvmAppData* GetAppByAccountId(const AccountId& account_id) const;

  // KioskAppManagerBase:
  std::vector<App> GetApps() const override;

  // Update Android Kiosk app display name and icon values stored in cache.
  void UpdateNameAndIcon(const AccountId& account_id,
                         const std::string& name,
                         const gfx::ImageSkia& icon);

  // Adds an app with the given meta data directly, skips meta data fetching
  // and sets the app as the auto launched one. Only for test.
  void AddAutoLaunchAppForTest(const std::string& app_id,
                               const policy::ArcvmKioskAppBasicInfo& app_info,
                               const AccountId& account_id);

  // If the app was auto launched, returns auto launched account id, otherwise
  // returns empty account id.
  const AccountId& GetAutoLaunchAccountId() const;

  // Returns the list of all apps in their internal representation.
  std::vector<const KioskArcvmAppData*> GetAppsForTesting() const;

 private:
  // KioskAppmanagerBase:
  // Updates `apps_` based on CrosSettings.
  void UpdateAppsFromPolicy() override;

  std::vector<std::unique_ptr<KioskArcvmAppData>> apps_;
  AccountId auto_launch_account_id_;
  // TODO(crbug.com/418940675): See if this can be refactored to raw_ref.
  const raw_ptr<PrefService> local_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_MANAGER_H_
