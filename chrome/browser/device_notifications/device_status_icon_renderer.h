// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_RENDERER_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_RENDERER_H_

#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/ui/chrome_pages.h"

class DeviceStatusIconRenderer : public DeviceSystemTrayIconRenderer,
                                 public StatusIconMenuModel::Delegate,
                                 public ProfileAttributesStorageObserver {
 public:
  explicit DeviceStatusIconRenderer(
      DeviceSystemTrayIcon* device_system_tray_icon,
      const chrome::HelpSource help_source,
      const int about_device_message_id);
  DeviceStatusIconRenderer(const DeviceStatusIconRenderer&) = delete;
  DeviceStatusIconRenderer& operator=(const DeviceStatusIconRenderer&) = delete;
  ~DeviceStatusIconRenderer() override;

  // DeviceSystemTrayIconRenderer
  void AddProfile(Profile* profile) override;
  void RemoveProfile(Profile* profile) override;
  void NotifyConnectionUpdated(Profile* profile) override;

  void ExecuteCommandForTesting(int command_id, int event_flags) {
    ExecuteCommand(command_id, event_flags);
  }

 private:
  // Returns a label for About device button.
  std::u16string GetAboutDeviceLabel();

  // Show the help center article for the device category.
  void ShowHelpCenterUrl();
  void ShowContentSettings(base::WeakPtr<Profile> profile);
  void ShowSiteSettings(base::WeakPtr<Profile> profile,
                        const url::Origin& origin);

  // ProfileAttributesStorage::Observer
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

  // Add a new menu item with the label |label| and the click handler |callback|
  // to the |menu|.
  void AddItem(StatusIconMenuModel* menu,
               std::u16string label,
               base::RepeatingClosure callback);

  void RefreshIcon();

  chrome::HelpSource help_source_;

  int about_device_message_id_;

  // Reference to our status icon (if any) - owned by the StatusTray.
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_ = nullptr;

  // The mapping of clickable system tray icon items to their click handlers
  std::vector<base::RepeatingClosure> command_id_callbacks_;

  base::WeakPtrFactory<DeviceStatusIconRenderer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_RENDERER_H_
