// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_BASE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace chromeos {

class KioskAppDataBase;
class KioskAppManagerObserver;

// Common base class for kiosk app managers.
class KioskAppManagerBase : public KioskAppDataDelegate {
 public:
  struct App {
    explicit App(const KioskAppDataBase&);
    App();
    App(const App&);
    ~App();

    std::string app_id;
    AccountId account_id;
    std::string name;
    gfx::ImageSkia icon;
    GURL url;  // Install url for web kiosk apps
    std::string required_platform_version;
    bool is_loading = false;
    bool was_auto_launched_with_zero_delay = false;
  };
  using AppList = std::vector<App>;

  KioskAppManagerBase();
  ~KioskAppManagerBase() override;

  // Depends on the app internal representation for the particular type of
  // kiosk.
  virtual void GetApps(AppList* apps) const = 0;

  void AddObserver(KioskAppManagerObserver* observer);
  void RemoveObserver(KioskAppManagerObserver* observer);

  // KioskAppDataDelegate overrides:
  void GetKioskAppIconCacheDir(base::FilePath* cache_dir) const override;
  void OnKioskAppDataChanged(const std::string& app_id) const override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) const override;

  void NotifyKioskAppsChanged() const;

  bool current_app_was_auto_launched_with_zero_delay() const {
    return auto_launched_with_zero_delay_;
  }

 protected:
  // Updates internal list of apps by the new data received by policy.
  virtual void UpdateAppsFromPolicy() = 0;

  // Performs removal of the removed apps's cryptohomes.
  void ClearRemovedApps(const std::vector<KioskAppDataBase*>& old_apps);

  bool auto_launched_with_zero_delay_ = false;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;

  base::ObserverList<KioskAppManagerObserver, true>::Unchecked observers_;

  base::WeakPtrFactory<KioskAppManagerBase> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KioskAppManagerBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_BASE_H_
