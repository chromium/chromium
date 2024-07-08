// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_connection_tracker.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/ui/chrome_pages.h"

HidConnectionTracker::HidConnectionTracker(Profile* profile)
    : DeviceConnectionTracker(profile) {
  whitelisted_origins_.push_back(url::Origin::Create(
      GURL("chrome-extension://ckcendljdlmgnhghiaomidhiiclmapok")));
  whitelisted_origins_.push_back(url::Origin::Create(
      GURL("chrome-extension://lfboplenmmjcmpbkeemecobbadnmpfhi")));
}

HidConnectionTracker::~HidConnectionTracker() = default;

void HidConnectionTracker::ShowContentSettingsExceptions() {
  chrome::ShowContentSettingsExceptionsForProfile(
      profile_, ContentSettingsType::HID_CHOOSER_DATA);
}

DeviceSystemTrayIcon* HidConnectionTracker::GetSystemTrayIcon() {
  return static_cast<DeviceSystemTrayIcon*>(
      g_browser_process->hid_system_tray_icon());
}

void HidConnectionTracker::Shutdown() {
  CleanUp();
  DeviceConnectionTracker::Shutdown();
}
