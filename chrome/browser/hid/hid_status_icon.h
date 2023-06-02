// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_STATUS_ICON_H_
#define CHROME_BROWSER_HID_HID_STATUS_ICON_H_

#include <memory>
#include <string>
#include <vector>
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "url/origin.h"

class HidStatusIcon : public HidSystemTrayIcon,
                      public StatusIconMenuModel::Delegate,
                      public ProfileAttributesStorage::Observer {
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

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

  static void ShowContentSettings(base::WeakPtr<Profile> profile);
  static void ShowHelpCenterUrl();
  static void ShowSiteSettings(base::WeakPtr<Profile> profile,
                               const url::Origin& origin);

  // Add a new menu item with the label |label| and the click handler |callback|
  // to the |menu|.
  void AddItem(StatusIconMenuModel* menu,
               std::u16string label,
               base::RepeatingClosure callback);

  // To refresh the system tray icon when there is a button (for a profile)
  // added/removed.
  void RefreshIcon();

  // Overrides from ProfileAttributesStorage::Observer
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

  // Reference to our status icon (if any) - owned by the StatusTray.
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_ = nullptr;

  // The mapping of clickable system tray icon items to their click handlers
  std::vector<base::RepeatingClosure> command_id_callbacks_;
};

#endif  // CHROME_BROWSER_HID_HID_STATUS_ICON_H_
