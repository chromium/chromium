// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_BASE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_BASE_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace ash {

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
  KioskAppManagerBase(const KioskAppManagerBase&) = delete;
  KioskAppManagerBase& operator=(const KioskAppManagerBase&) = delete;
  ~KioskAppManagerBase() override;

  // Depends on the app internal representation for the particular type of
  // kiosk.
  virtual AppList GetApps() const = 0;

  void AddObserver(KioskAppManagerObserver* observer);
  void RemoveObserver(KioskAppManagerObserver* observer);

  // KioskAppDataDelegate overrides:
  void GetKioskAppIconCacheDir(base::FilePath* cache_dir) override;
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnExternalCacheDamaged(const std::string& app_id) override;

  // Gets whether the bailout shortcut is disabled.
  bool GetDisableBailoutShortcut() const;

  bool current_app_was_auto_launched_with_zero_delay() const {
    return auto_launched_with_zero_delay_;
  }

  void set_current_app_was_auto_launched_with_zero_delay_for_testing(
      bool value) {
    auto_launched_with_zero_delay_ = value;
  }

 protected:
  // Notifies the observers about the updates.
  void NotifyKioskAppsChanged() const;
  void NotifySessionInitialized() const;

  // Updates internal list of apps by the new data received by policy.
  virtual void UpdateAppsFromPolicy() = 0;

  // Performs removal of the removed apps's cryptohomes.
  void ClearRemovedApps(const std::vector<KioskAppDataBase*>& old_apps);

  bool auto_launched_with_zero_delay_ = false;

  base::CallbackListSubscription local_accounts_subscription_;
  base::CallbackListSubscription local_account_auto_login_id_subscription_;

  base::ObserverList<KioskAppManagerObserver, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<KioskAppManagerBase> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_MANAGER_BASE_H_
