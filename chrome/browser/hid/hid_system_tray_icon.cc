// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon.h"

#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

bool ContainsProfile(const std::vector<base::WeakPtr<Profile>>& profiles,
                     Profile* profile) {
  return base::ranges::count_if(profiles, [profile](const auto& entry) {
           return entry && entry.get() == profile;
         }) > 0;
}

size_t EraseProfile(std::vector<base::WeakPtr<Profile>>& profiles,
                    Profile* profile) {
  return base::EraseIf(profiles, [profile](const auto& entry) {
    return entry && entry.get() == profile;
  });
}

}  // namespace

// static
gfx::ImageSkia HidSystemTrayIcon::GetStatusTrayIcon() {
  return gfx::CreateVectorIcon(vector_icons::kVideogameAssetIcon,
                               gfx::kGoogleGrey300);
}

// static
std::u16string HidSystemTrayIcon::GetManageHidDeviceButtonLabel(
    Profile* profile) {
  std::u16string profile_name =
      base::UTF8ToUTF16(profile->GetProfileUserName());
  if (profile_name.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_BUTTON_FOR_MANAGE_HID_DEVICE);
  }
  return l10n_util::GetStringFUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_BUTTON_FOR_MANAGE_HID_DEVICE_WITH_PROFILE_NAME,
      profile_name);
}

// static
std::u16string HidSystemTrayIcon::GetTooltipLabel(size_t num_devices) {
  return l10n_util::GetPluralStringFUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_TOOLTIP,
                                          static_cast<int>(num_devices));
}

HidSystemTrayIcon::HidSystemTrayIcon() = default;
HidSystemTrayIcon::~HidSystemTrayIcon() = default;

void HidSystemTrayIcon::StageProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (EraseProfile(unstaging_profiles_, profile) > 0) {
    // Connection tracker's connection count is updated even the profile is just
    // moved from unstaging to staging.
    NotifyConnectionCountUpdated(profile);
    return;
  }
  AddProfile(profile);
}

void HidSystemTrayIcon::UnstageProfile(Profile* profile, bool immediate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (immediate) {
    RemoveProfile(profile);
    EraseProfile(unstaging_profiles_, profile);
    return;
  }
  if (ContainsProfile(unstaging_profiles_, profile)) {
    return;
  }
  // In order to avoid bouncing the system tray icon, schedule |profile| to be
  // removed from the system tray icon later.
  unstaging_profiles_.push_back(profile->GetWeakPtr());
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostDelayedTask(
          FROM_HERE,
          // This class is supposedly safe as it is owned by
          // g_browser_process. However, to avoid corner
          // scenarios in tests, use weak ptr just to be safe.
          base::BindOnce(&HidSystemTrayIcon::CleanUpProfiles,
                         weak_factory_.GetWeakPtr(), profile->GetWeakPtr()),
          kProfileUnstagingTime);
  // Connection tracker's connection count is updated even in scheduled
  // removal case.
  NotifyConnectionCountUpdated(profile);
}

void HidSystemTrayIcon::CleanUpProfiles(base::WeakPtr<Profile> profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile) {
    if (EraseProfile(unstaging_profiles_, profile.get()) > 0) {
      RemoveProfile(profile.get());
    }
    return;
  }
  // When removing |profile| from |unstaging_profiles_|, cleans up other
  // destroyed profiles too as it loops through the |unstaging_profiles_|.
  base::EraseIf(unstaging_profiles_, [](const auto& entry) { return !entry; });
}
