// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_
#define CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_

#include "chrome/browser/hid/hid_system_tray_icon.h"

class HidPinnedNotification : public HidSystemTrayIcon {
 public:
  HidPinnedNotification();
  HidPinnedNotification(const HidPinnedNotification&) = delete;
  HidPinnedNotification& operator=(const HidPinnedNotification&) = delete;
  ~HidPinnedNotification() override;
};

#endif  // CHROME_BROWSER_HID_HID_PINNED_NOTIFICATION_H_
