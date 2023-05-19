// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

class Profile;

class HidSystemTrayIcon : public DeviceSystemTrayIcon {
 public:
  HidSystemTrayIcon();
  HidSystemTrayIcon(const HidSystemTrayIcon&) = delete;
  HidSystemTrayIcon& operator=(const HidSystemTrayIcon&) = delete;
  ~HidSystemTrayIcon() override;

  void StageProfile(Profile* profile) override;
  void UnstageProfile(Profile* profile, bool immediate) override;

  // The time period that a profile is shown in the system tray icon while it is
  // unstaging.
  static constexpr base::TimeDelta kProfileUnstagingTime = base::Seconds(10);

  const base::flat_map<Profile*, bool>& GetProfilesForTesting() {
    return profiles_;
  }

 protected:
  // Get the image for the status tray icon.
  static gfx::ImageSkia GetStatusTrayIcon();

  // Get the label of the title of the HID system tray icon.
  static std::u16string GetTitleLabel(size_t num_origins,
                                      size_t num_connections);

  // Returns a label for HID settings button.
  static std::u16string GetContentSettingsLabel();

  // This map stores profiles being tracked, along with their staging status.
  base::flat_map<Profile*, bool> profiles_;

 private:
  // Remove |profile| from the system tray icon if it is still unstaging.
  void CleanUpProfile(base::WeakPtr<Profile> profile);

  base::WeakPtrFactory<HidSystemTrayIcon> weak_factory_{this};
};

#endif  // CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
