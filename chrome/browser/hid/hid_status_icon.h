// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_STATUS_ICON_H_
#define CHROME_BROWSER_HID_HID_STATUS_ICON_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/status_icons/status_icon.h"

class HidStatusIcon : public HidSystemTrayIcon,
                      public StatusIconMenuModel::Delegate {
 public:
  HidStatusIcon();
  HidStatusIcon(const HidStatusIcon&) = delete;
  HidStatusIcon& operator=(const HidStatusIcon&) = delete;
  ~HidStatusIcon() override;

  void AddProfile(Profile* profile) override;
  void RemoveProfile(Profile* profile) override;
  void NotifyConnectionCountUpdated(Profile* profile) override;

 private:
  // For using ExecuteCommand to simulate button click.
  friend class WebHidExtensionBrowserTest;

  // Get the total connection count from all the profiles being tracked.
  size_t GetTotalConnectionCount();

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

  // To refresh the system tray icon when there is a button (for a profile)
  // added/removed.
  void RefreshIcon();

  // Reference to our status icon (if any) - owned by the StatusTray.
  raw_ptr<StatusIcon> status_icon_ = nullptr;

  // A list of profiles being tracked, each profile has an entry in the context
  // menu of the system tray icon. Each entry in |profiles_| is expected to be
  // maintained by the profile's HidConnectionTracker. Meaning
  // HidConnectionTracker is responsible for removing the profile from
  // |profiles_| by calling RemoveProfile when the profile is about to be
  // destroyed, so that there is never a case where a destroyed profile can be
  // accessed through |profiles_|.
  std::vector<Profile*> profiles_;
};

#endif  // CHROME_BROWSER_HID_HID_STATUS_ICON_H_
