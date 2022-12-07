// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_

#include "chrome/browser/ash/app_mode/metrics/low_disk_metrics_service.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"

namespace ash {

class NetworkConnectivityMetricsService;

// AppSessionAsh maintains a kiosk session and handles its lifetime.
class AppSessionAsh : public chromeos::AppSession {
 public:
  explicit AppSessionAsh(Profile* profile);
  AppSessionAsh(const AppSessionAsh&) = delete;
  AppSessionAsh& operator=(const AppSessionAsh&) = delete;
  ~AppSessionAsh() override;

  // chromeos::AppSession:
  void Init(const std::string& app_id) override;
  void InitForWebKiosk(
      const absl::optional<std::string>& web_app_name) override;

  // Destroys ash observers.
  void ShuttingDown();

 private:
  // Initialize the Kiosk app update service. The external update will be
  // triggered if a USB stick is used.
  void InitKioskAppUpdateService(const std::string& app_id);

  // If the device is not enterprise managed, set prefs to reboot after update
  // and create a user security message which shows the user the application
  // name and author after some idle timeout.
  void SetRebootAfterUpdateIfNecessary();

  // Tracks network connectivity drops.
  // Init in ctor and destroyed while ShuttingDown.
  std::unique_ptr<NetworkConnectivityMetricsService> network_metrics_service_;

  const std::unique_ptr<PeriodicMetricsService> periodic_metrics_service_;

  // Tracks low disk notifications.
  LowDiskMetricsService low_disk_metrics_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
