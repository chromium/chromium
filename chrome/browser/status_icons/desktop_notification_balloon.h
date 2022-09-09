// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
#define CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_

#include <memory>
#include <string>

namespace message_center {
struct NotifierId;
}

namespace ui {
class ImageModel;
}

// Provides the notification balloon functionality by using desktop
// notifications to platforms that don't have a specific native API.
class DesktopNotificationBalloon {
 public:
  DesktopNotificationBalloon();

  DesktopNotificationBalloon(const DesktopNotificationBalloon&) = delete;
  DesktopNotificationBalloon& operator=(const DesktopNotificationBalloon&) =
      delete;

  virtual ~DesktopNotificationBalloon();

  void DisplayBalloon(const ui::ImageModel& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id);

 private:
  // Counter to provide unique ids to notifications.
  static int id_count_;
};

#endif  // CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
