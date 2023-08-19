// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_RENDERER_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_RENDERER_H_

#include "chrome/browser/profiles/profile.h"

class DeviceSystemTrayIcon;

class DeviceSystemTrayIconRenderer {
 public:
  explicit DeviceSystemTrayIconRenderer(
      DeviceSystemTrayIcon* device_system_tray_icon)
      : device_system_tray_icon_(device_system_tray_icon) {}
  explicit DeviceSystemTrayIconRenderer(const DeviceSystemTrayIcon&) = delete;
  DeviceSystemTrayIconRenderer& operator=(const DeviceSystemTrayIcon&) = delete;
  virtual ~DeviceSystemTrayIconRenderer() = default;

  virtual void AddProfile(Profile* profile) = 0;
  virtual void RemoveProfile(Profile* profile) = 0;
  virtual void NotifyConnectionUpdated(Profile* profile) = 0;

 protected:
  // DeviceSystemTrayIconRenderer is owned by `device_system_tray_icon_`
  raw_ptr<DeviceSystemTrayIcon> device_system_tray_icon_;
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_RENDERER_H_
