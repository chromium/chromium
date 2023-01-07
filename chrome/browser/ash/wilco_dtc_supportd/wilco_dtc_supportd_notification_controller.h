// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "ui/message_center/public/cpp/notification.h"

class ProfileManager;

namespace ash {

// Controller class to manage wilco notification.
class WilcoDtcSupportdNotificationController {
 public:
  WilcoDtcSupportdNotificationController();
  explicit WilcoDtcSupportdNotificationController(
      ProfileManager* profile_manager);
  virtual ~WilcoDtcSupportdNotificationController();
  WilcoDtcSupportdNotificationController(
      const WilcoDtcSupportdNotificationController& other) = delete;
  WilcoDtcSupportdNotificationController& operator=(
      const WilcoDtcSupportdNotificationController& other) = delete;

  // Displays notification when an unauthorized battery is connected.
  virtual std::string ShowBatteryAuthNotification() const;
  // Displays notification when an unauthorized charger is used, and battery
  // will not charge.
  virtual std::string ShowNonWilcoChargerNotification() const;
  // Displays notification when the attached dock is incompatible.
  virtual std::string ShowIncompatibleDockNotification() const;
  // Displays notification when the attached dock presents hardware failures.
  virtual std::string ShowDockErrorNotification() const;
  // Displays notification when HDMI and USB Type-C are used for displays at the
  // same time with the attached dock.
  virtual std::string ShowDockDisplayNotification() const;
  // Displays notification when the attached dock has unsupported Thunderbolt
  // capabilities.
  virtual std::string ShowDockThunderboltNotification() const;
  // Displays notification when the attached charger does not supply enough
  // power.
  virtual std::string ShowLowPowerChargerNotification() const;

 private:
  void DisplayNotification(
      const std::string& notification_id,
      const NotificationCatalogName& catalog_name,
      const int title_id,
      const int message_id,
      const message_center::NotificationPriority priority,
      const gfx::VectorIcon& small_image,
      const message_center::SystemNotificationWarningLevel color_type,
      const HelpAppLauncher::HelpTopic topic) const;

  ProfileManager* profile_manager_;  // non-owned
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NOTIFICATION_CONTROLLER_H_
