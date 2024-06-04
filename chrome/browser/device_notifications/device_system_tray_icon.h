// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/device_notifications/device_connection_tracker.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"

class Profile;
class DeviceSystemTrayIconRenderer;
class DeviceSystemTrayIcon {
 public:
  explicit DeviceSystemTrayIcon(
      std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer);
  DeviceSystemTrayIcon(const DeviceSystemTrayIcon&) = delete;
  DeviceSystemTrayIcon& operator=(const DeviceSystemTrayIcon&) = delete;
  virtual ~DeviceSystemTrayIcon();

  // Stage `profile` to be shown in the system tray icon.
  virtual void StageProfile(Profile* profile);

  // TODO(crbug.com/40235090): Remove support for non-immediate unstage request.
  // Unstage `profile` that is being shown in the system tray icon. The profile
  // will be removed immediately when `immediate` is true, otherwise it is
  // scheduled to be removed later.
  virtual void UnstageProfile(Profile* profile, bool immediate);

  // Notify the system tray icon the connection count of the `profile` has
  // changed.
  virtual void NotifyConnectionCountUpdated(Profile* profile);

  // Get the image for the status tray icon.
  virtual const gfx::VectorIcon& GetIcon() = 0;

  // Get the label of the title of the device system tray icon.
  virtual std::u16string GetTitleLabel(size_t num_origins,
                                       size_t num_connections) = 0;

  // Returns a label for the content setting button
  virtual std::u16string GetContentSettingsLabel() = 0;

  // The time period that a profile is shown in the system tray icon while it is
  // unstaging.
  static constexpr base::TimeDelta kProfileUnstagingTime = base::Seconds(10);

  const base::flat_map<Profile*, bool>& GetProfilesForTesting() {
    return profiles_;
  }

  DeviceSystemTrayIconRenderer* GetIconRendererForTesting() {
    return icon_renderer_.get();
  }

  virtual DeviceConnectionTracker* GetConnectionTracker(
      base::WeakPtr<Profile> profile) = 0;

  const base::flat_map<Profile*, bool>& profiles() const { return profiles_; }

 protected:
  std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer_;

 private:
  // This function is called after the `profile` object is added to the
  // `profiles_`.
  virtual void ProfileAdded(Profile* profile);

  // This function is called after the `profile` object is removed from the
  // `profiles_`.
  virtual void ProfileRemoved(Profile* profile);

  // Remove |profile| from the system tray icon if it is still unstaging.
  void CleanUpProfile(base::WeakPtr<Profile> profile);

  base::flat_map<Profile*, bool> profiles_;

  base::WeakPtrFactory<DeviceSystemTrayIcon> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_SYSTEM_TRAY_ICON_H_
