// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_

#include <memory>

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/metrics/low_disk_metrics_service.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class NetworkConnectivityMetricsService;

// AppSessionAsh maintains a kiosk session and handles its lifetime.
class AppSessionAsh {
 public:
  explicit AppSessionAsh(
      Profile* profile,
      const KioskAppId& kiosk_app_id,
      const absl::optional<std::string>& app_name = absl::nullopt);
  AppSessionAsh(const AppSessionAsh&) = delete;
  AppSessionAsh& operator=(const AppSessionAsh&) = delete;
  ~AppSessionAsh();

  // Destroys ash observers.
  void ShuttingDown();

  void OnGuestAdded(content::WebContents* guest_web_contents);

  bool is_shutting_down() const;

  Browser* GetSettingsBrowserForTesting();

  void SetOnHandleBrowserCallbackForTesting(
      base::RepeatingCallback<void(bool)> callback);

 private:
  class LacrosWatcher;

  void InitForChromeAppKiosk();
  void InitForWebKiosk(const absl::optional<std::string>& app_name);

  // Initialize the Kiosk app update service. The external update will be
  // triggered if a USB stick is used.
  void InitKioskAppUpdateService(const std::string& app_id);

  // If the device is not enterprise managed, set prefs to reboot after update
  // and create a user security message which shows the user the application
  // name and author after some idle timeout.
  void SetRebootAfterUpdateIfNecessary();

  Profile* profile() const;

  // Owned by `ProfileManager`.
  raw_ptr<Profile> profile_ = nullptr;

  chromeos::AppSession app_session_;

  const KioskAppId kiosk_app_id_;

  // Tracks network connectivity drops.
  // Init in ctor and destroyed while ShuttingDown.
  std::unique_ptr<NetworkConnectivityMetricsService> network_metrics_service_;

  const std::unique_ptr<PeriodicMetricsService> periodic_metrics_service_;
  std::unique_ptr<LacrosWatcher> lacros_watcher_;

  // Tracks low disk notifications.
  LowDiskMetricsService low_disk_metrics_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
