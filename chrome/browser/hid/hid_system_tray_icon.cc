// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon.h"

#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

// static
gfx::ImageSkia HidSystemTrayIcon::GetStatusTrayIcon() {
  return gfx::CreateVectorIcon(vector_icons::kVideogameAssetIcon,
                               gfx::kGoogleGrey300);
}

// static
std::u16string HidSystemTrayIcon::GetTitleLabel(size_t num_origins,
                                                size_t num_connections) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (num_origins == 1) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_SINGLE_EXTENSION,
        static_cast<int>(num_connections));
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_MULTIPLE_EXTENSIONS,
      static_cast<int>(num_connections));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

// static
std::u16string HidSystemTrayIcon::GetContentSettingsLabel() {
  return l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_HID_SETTINGS);
}

HidSystemTrayIcon::HidSystemTrayIcon() = default;
HidSystemTrayIcon::~HidSystemTrayIcon() = default;

void HidSystemTrayIcon::StageProfile(Profile* profile) {
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

void HidSystemTrayIcon::UnstageProfile(Profile* profile, bool immediate) {
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
          base::BindOnce(&HidSystemTrayIcon::CleanUpProfile,
                         weak_factory_.GetWeakPtr(), profile->GetWeakPtr()),
          kProfileUnstagingTime);
  // Connection tracker's connection count is updated even in scheduled
  // removal case.
  NotifyConnectionCountUpdated(profile);
}

void HidSystemTrayIcon::CleanUpProfile(base::WeakPtr<Profile> profile) {
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
  // |profile|. This is because HidConnectionTracker::CleanUp() is called on
  // browser context (i.e. profile) shutdown and calls UnstageProfile() with
  // immediate set to true so the entry will be removed from |profiles_|
  // immediately.
}

bool HidSystemTrayIcon::ContainProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return profiles_.contains(profile);
}
