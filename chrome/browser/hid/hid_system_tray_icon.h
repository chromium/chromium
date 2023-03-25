// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

class HidSystemTrayIcon {
 public:
  HidSystemTrayIcon();
  HidSystemTrayIcon(const HidSystemTrayIcon&) = delete;
  HidSystemTrayIcon& operator=(const HidSystemTrayIcon&) = delete;
  virtual ~HidSystemTrayIcon();

  // Stage |profile| to be shown in the system tray icon.
  virtual void StageProfile(Profile* profile);

  // Unstage |profile| that is being shown in the system tray icon. The profile
  // will be removed immediately when |immediate| is true, otherwise it is
  // scheduled to be removed later.
  virtual void UnstageProfile(Profile* profile, bool immediate);

  // Notify the system tray icon the connection count of the |profile| has
  // changed.
  virtual void NotifyConnectionCountUpdated(Profile* profile) = 0;

  // The time period that a profile is shown in the system tray icon while it is
  // unstaging.
  static constexpr base::TimeDelta kProfileUnstagingTime = base::Seconds(10);

 protected:
  // Get the image for the status tray icon.
  static gfx::ImageSkia GetStatusTrayIcon();

  // Get the label of the button for managing HID device permission on the HID
  // system tray icon.
  static std::u16string GetManageHidDeviceButtonLabel(Profile* profile);

  // Get the label of the tooltip of the HID system tray icon.
  static std::u16string GetTooltipLabel(size_t num_devices);

 private:
  FRIEND_TEST_ALL_PREFIXES(HidSystemTrayIconTest, UnstageProfile);
  FRIEND_TEST_ALL_PREFIXES(HidSystemTrayIconTest,
                           CallbackAfterHidSystemTrayIconDestroyed);

  // Add a profile to the system tray icon.
  virtual void AddProfile(Profile* profile) = 0;

  // Remove a profile from the system tray icon.
  virtual void RemoveProfile(Profile* profile) = 0;

  // Remove |profile| from the system tray icon if it is still in
  // |unstaging_profiles_|.
  void CleanUpProfiles(base::WeakPtr<Profile> profile);

  // A list of profiles that are unstaging, which are scheduled to be removed.
  // later.
  std::vector<base::WeakPtr<Profile>> unstaging_profiles_;

  base::WeakPtrFactory<HidSystemTrayIcon> weak_factory_{this};
};

#endif  // CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
