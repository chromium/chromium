// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_STATUS_ICON_H_
#define CHROME_BROWSER_HID_HID_STATUS_ICON_H_

#include "chrome/browser/hid/hid_system_tray_icon.h"

class HidStatusIcon : public HidSystemTrayIcon {
 public:
  HidStatusIcon();
  HidStatusIcon(const HidStatusIcon&) = delete;
  HidStatusIcon& operator=(const HidStatusIcon&) = delete;
  ~HidStatusIcon() override;
};

#endif  // CHROME_BROWSER_HID_HID_STATUS_ICON_H_
