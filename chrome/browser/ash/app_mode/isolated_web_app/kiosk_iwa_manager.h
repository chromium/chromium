// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_

#include <memory>
#include <vector>

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
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

 private:
  void UpdateAppsFromPolicy() override;

  std::vector<std::unique_ptr<KioskIwaData>> isolated_web_apps_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_MANAGER_H_
