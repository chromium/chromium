// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace chromeos {

// Keeps track of Android apps that are to be launched in kiosk mode.
// For removed apps deletes appropriate cryptohome. The information about
// kiosk apps are received from CrosSettings. For each app, the system
// creates a user in whose context the app then runs.
class ArcKioskAppManager : public KioskAppManagerBase {
 public:
  static const char kArcKioskDictionaryName[];

  static ArcKioskAppManager* Get();
  ArcKioskAppManager();
  ~ArcKioskAppManager() override;

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns app that should be started for given account id.
  const ArcKioskAppData* GetAppByAccountId(const AccountId& account_id);

  // KioskAppManagerBase:
  void GetApps(std::vector<App>* apps) const override;

  void UpdateNameAndIcon(const std::string& app_id,
                         const std::string& name,
                         const gfx::ImageSkia& icon);

  // Adds an app with the given meta data directly, skips meta data fetching
  // and sets the app as the auto launched one. Only for test.
  void AddAutoLaunchAppForTest(const std::string& app_id,
                               const policy::ArcKioskAppBasicInfo& app_info,
                               const AccountId& account_id);

  // If the app was auto launched, returns auto launched account id, otherwise
  // returns empty account id.
  const AccountId& GetAutoLaunchAccountId() const;

 private:
  friend class ArcKioskAppManagerTest;
  // Returns the list of all apps in their internal representation.
  void GetAppsForTesting(
      std::vector<const ArcKioskAppData*>* apps_internal) const;

  // KioskAppmanagerBase:
  // Updates |apps_| based on CrosSettings.
  void UpdateAppsFromPolicy() override;

  std::vector<std::unique_ptr<ArcKioskAppData>> apps_;
  AccountId auto_launch_account_id_;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_
