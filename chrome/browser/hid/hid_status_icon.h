// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_STATUS_ICON_H_
#define CHROME_BROWSER_HID_HID_STATUS_ICON_H_

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

  void NotifyConnectionCountUpdated(Profile* profile) override;

 private:
  // For using ExecuteCommand to simulate button click.
  friend class WebHidExtensionBrowserTest;

  void ProfileAdded(Profile* profile) override;
  void ProfileRemoved(Profile* profile) override;

  // Get the total connection count from all the profiles being tracked.
  size_t GetTotalConnectionCount();

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

  // To refresh the system tray icon when there is a button (for a profile)
  // added/removed.
  void RefreshIcon();

  // Reference to our status icon (if any) - owned by the StatusTray.
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_ = nullptr;

  // A list of profiles currently shown on the status icon.
  std::vector<Profile*> visible_profiles_;
};

#endif  // CHROME_BROWSER_HID_HID_STATUS_ICON_H_
