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

  void AddProfile(Profile* profile) override;
  void RemoveProfile(Profile* profile) override;
  void NotifyConnectionCountUpdated(Profile* profile) override;

  static std::string GetNotificationId(Profile* profile);

 private:
  // Create a pinned notification for |profile| to indicate at least one HID
  // device is being accessed.
  std::unique_ptr<message_center::Notification> CreateNotification(
      Profile* profile);

  // Display |notification| in the system notification.
  void DisplayNotification(
      std::unique_ptr<message_center::Notification> notification);

  // A set of profiles being tracked, each profile have a pinned notification in
  // the system tray.
  base::flat_set<Profile*> profiles_;
};

#endif  // CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_
