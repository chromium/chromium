// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
#define CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_

#include "chrome/browser/device_notifications/device_connection_tracker.h"

// Manages the opened device connection count by the profile.
class HidConnectionTracker : public DeviceConnectionTracker {
 public:
  explicit HidConnectionTracker(Profile* profile);
  HidConnectionTracker(HidConnectionTracker&&) = delete;
  HidConnectionTracker& operator=(HidConnectionTracker&) = delete;
  ~HidConnectionTracker() override;

  void ShowContentSettingsExceptions() override;

  // KeyedService:
  void Shutdown() override;

 private:
  DeviceSystemTrayIcon* GetSystemTrayIcon() override;
};

#endif  // CHROME_BROWSER_HID_HID_CONNECTION_TRACKER_H_
