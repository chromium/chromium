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
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

class Profile;

class HidSystemTrayIcon {
 public:
  HidSystemTrayIcon();
  HidSystemTrayIcon(const HidSystemTrayIcon&) = delete;
  HidSystemTrayIcon& operator=(const HidSystemTrayIcon&) = delete;
  virtual ~HidSystemTrayIcon();

  // Stage |profile| to be shown in the system tray icon.
  virtual void StageProfile(Profile* profile);

  // TODO(crbug.com/1353104): Remove support for non-immediate unstage request.
  // Unstage |profile| that is being shown in the system tray icon. The profile
  // will be removed immediately when |immediate| is true, otherwise it is
  // scheduled to be removed later.
  virtual void UnstageProfile(Profile* profile, bool immediate);

  // Notify the system tray icon the connection count of the |profile| has
  // changed.
  virtual void NotifyConnectionCountUpdated(Profile* profile) = 0;

  // Check if the |profile| is being tracked by the system tray icon.
  virtual bool ContainProfile(Profile* profile);

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
  // This function is called after the |profile| object is added to the
  // |profiles_|.
  virtual void ProfileAdded(Profile* profile) = 0;

  // This function is called after the |profile| object is removed from the
  // |profiles_|.
  virtual void ProfileRemoved(Profile* profile) = 0;

  // Remove |profile| from the system tray icon if it is still unstaging.
  void CleanUpProfile(base::WeakPtr<Profile> profile);

  base::WeakPtrFactory<HidSystemTrayIcon> weak_factory_{this};
};

#endif  // CHROME_BROWSER_HID_HID_SYSTEM_TRAY_ICON_H_
