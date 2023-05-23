// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class Profile;

class DeviceSystemTrayIcon {
 public:
  DeviceSystemTrayIcon();
  DeviceSystemTrayIcon(const DeviceSystemTrayIcon&) = delete;
  DeviceSystemTrayIcon& operator=(const DeviceSystemTrayIcon&) = delete;
  virtual ~DeviceSystemTrayIcon();

  // Stage `profile` to be shown in the system tray icon.
  virtual void StageProfile(Profile* profile);

  // TODO(crbug.com/1353104): Remove support for non-immediate unstage request.
  // Unstage `profile` that is being shown in the system tray icon. The profile
  // will be removed immediately when `immediate` is true, otherwise it is
  // scheduled to be removed later.
  virtual void UnstageProfile(Profile* profile, bool immediate);

  // Notify the system tray icon the connection count of the `profile` has
  // changed.
  virtual void NotifyConnectionCountUpdated(Profile* profile) = 0;

  // The time period that a profile is shown in the system tray icon while it is
  // unstaging.
  static constexpr base::TimeDelta kProfileUnstagingTime = base::Seconds(10);

  const base::flat_map<Profile*, bool>& GetProfilesForTesting() {
    return profiles_;
  }

 protected:
  // This function is called after the `profile` object is added to the
  // `profiles_`.
  virtual void ProfileAdded(Profile* profile) = 0;

  // This function is called after the `profile` object is removed from the
  // `profiles_`.
  virtual void ProfileRemoved(Profile* profile) = 0;

  // This map stores profiles being tracked, along with their staging status.
  base::flat_map<Profile*, bool> profiles_;

 private:
  // Remove |profile| from the system tray icon if it is still unstaging.
  void CleanUpProfile(base::WeakPtr<Profile> profile);

  base::WeakPtrFactory<DeviceSystemTrayIcon> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_
