// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace arc {

// Implements the delegate interface for managing notifications related
// to ARC DLC installation. Responsible for displaying notifications
// in the Chrome OS environment.
class ArcDlcInstallNotificationManagerDelegateImpl
    : public ArcDlcInstallNotificationManager::Delegate {
 public:
  // Constructs a delegate for managing ARC DLC installation notifications.
  explicit ArcDlcInstallNotificationManagerDelegateImpl(Profile* profile);

  ~ArcDlcInstallNotificationManagerDelegateImpl() override;

  // Displays a notification using the notification system.
  void DisplayNotification(
      const message_center::Notification& notification) override;

 private:
  // The profile associated with the notifications
  // which is also the primary profile associated with the arc session
  const raw_ptr<Profile> profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_IMPL_H_
