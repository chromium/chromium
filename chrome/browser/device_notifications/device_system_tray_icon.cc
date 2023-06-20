// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

DeviceSystemTrayIcon::DeviceSystemTrayIcon(
    std::unique_ptr<DeviceSystemTrayIconRenderer> icon_renderer)
    : icon_renderer_(std::move(icon_renderer)) {}
DeviceSystemTrayIcon::~DeviceSystemTrayIcon() = default;

void DeviceSystemTrayIcon::StageProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = profiles_.find(profile);
  if (it != profiles_.end()) {
    // If the |profile| is tracked, it must be unstaging.
    CHECK(!it->second);
    // Connection tracker's connection count is updated even the profile is
    // just moved from unstaging to staging.
    NotifyConnectionCountUpdated(profile);
    it->second = true;
    return;
  }
  profiles_[profile] = true;
  ProfileAdded(profile);
}

void DeviceSystemTrayIcon::UnstageProfile(Profile* profile, bool immediate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = profiles_.find(profile);
  // The |profile| must be tracked. However, it can be unstaging. For example,
  // A profile is scheduled to be removed followed by profile destruction.
  CHECK(it != profiles_.end());

  if (immediate) {
    profiles_.erase(it);
    ProfileRemoved(profile);
    return;
  }

  // For non-immediate case, the |profile| must be staging.
  CHECK(it->second);
  it->second = false;
  // In order to avoid bouncing the system tray icon, schedule |profile| to be
  // removed from the system tray icon later.
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostDelayedTask(
          FROM_HERE,
          // This class is supposedly safe as it is owned by
          // g_browser_process. However, to avoid corner
          // scenarios in tests, use weak ptr just to be safe.
          base::BindOnce(&DeviceSystemTrayIcon::CleanUpProfile,
                         weak_factory_.GetWeakPtr(), profile->GetWeakPtr()),
          kProfileUnstagingTime);
  // Connection tracker's connection count is updated even in scheduled
  // removal case.
  NotifyConnectionCountUpdated(profile);
}

void DeviceSystemTrayIcon::ProfileAdded(Profile* profile) {
  if (icon_renderer_) {
    icon_renderer_->AddProfile(profile);
  }
}

void DeviceSystemTrayIcon::ProfileRemoved(Profile* profile) {
  if (icon_renderer_) {
    icon_renderer_->RemoveProfile(profile);
  }
}

void DeviceSystemTrayIcon::NotifyConnectionCountUpdated(Profile* profile) {
  if (icon_renderer_) {
    icon_renderer_->NotifyConnectionUpdated(profile);
  }
}

void DeviceSystemTrayIcon::CleanUpProfile(base::WeakPtr<Profile> profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile) {
    auto it = profiles_.find(profile.get());
    if (it != profiles_.end() && !it->second) {
      profiles_.erase(it);
      ProfileRemoved(profile.get());
    }
    return;
  }
  // If the |profile| is destroyed, |profiles_| shouldn't have an entry for
  // |profile|. This is because DeviceConnectionTracker::CleanUp() is called on
  // browser context (i.e. profile) shutdown and calls UnstageProfile() with
  // immediate set to true so the entry will be removed from |profiles_|
  // immediately.
}
