// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
#define CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace gfx {
class ImageSkia;
}

namespace message_center {
struct NotifierId;
}

// Provides the notification balloon functionality by using desktop
// notifications to platforms that don't have a specific native API.
class DesktopNotificationBalloon {
 public:
  DesktopNotificationBalloon();
  virtual ~DesktopNotificationBalloon();

  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id);

 private:
  // Counter to provide unique ids to notifications.
  static int id_count_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationBalloon);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
