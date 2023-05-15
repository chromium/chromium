// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_
#define CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "ui/message_center/public/cpp/notification.h"

class HidPinnedNotification : public HidSystemTrayIcon {
 public:
  HidPinnedNotification();
  HidPinnedNotification(const HidPinnedNotification&) = delete;
  HidPinnedNotification& operator=(const HidPinnedNotification&) = delete;
  ~HidPinnedNotification() override;

  void NotifyConnectionCountUpdated(Profile* profile) override;

  static std::string GetNotificationId(Profile* profile);

 private:
  void ProfileAdded(Profile* profile) override;
  void ProfileRemoved(Profile* profile) override;

  // Create a pinned notification for |profile| to indicate at least one HID
  // device is being accessed.
  std::unique_ptr<message_center::Notification> CreateNotification(
      Profile* profile);

  // Display |notification| in the system notification.
  void DisplayNotification(
      std::unique_ptr<message_center::Notification> notification);
};

#endif  // CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_
