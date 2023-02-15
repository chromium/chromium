// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_

#include <string>

#include "ui/gfx/image/image_skia.h"

class Profile;

class HidSystemTrayIcon {
 public:
  HidSystemTrayIcon() = default;
  HidSystemTrayIcon(const HidSystemTrayIcon&) = delete;
  HidSystemTrayIcon& operator=(const HidSystemTrayIcon&) = delete;
  virtual ~HidSystemTrayIcon() = default;

  // Add a profile to the system tray icon.
  virtual void AddProfile(Profile* profile) = 0;

  // Remove a profile from the system tray icon.
  virtual void RemoveProfile(Profile* profile) = 0;

  // Notify the system tray icon the connection count of the |profile| has
  // changed.
  virtual void NotifyConnectionCountUpdated(Profile* profile) = 0;

 protected:
  // Get the image for the status tray icon.
  static gfx::ImageSkia GetStatusTrayIcon();

  // Get the label of the button for managing HID device permission on the HID
  // system tray icon.
  static std::u16string GetManageHidDeviceButtonLabel(Profile* profile);

  // Get the label of the tooltip of the HID system tray icon.
  static std::u16string GetTooltipLabel(size_t num_devices);
};

#endif  // CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
