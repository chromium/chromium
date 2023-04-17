// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_connection_tracker.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

HidConnectionTracker::HidConnectionTracker(Profile* profile)
    : profile_(profile) {}

HidConnectionTracker::~HidConnectionTracker() {
  CleanUp();
}

void HidConnectionTracker::IncrementConnectionCount(const url::Origin& origin) {
  CHECK_GE(origins_[origin], 0);
  origins_[origin]++;
  total_connection_count_++;

  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }
  if (total_connection_count_ == 1) {
    hid_system_tray_icon->StageProfile(profile_);
  } else {
    hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
  }
}

void HidConnectionTracker::DecrementConnectionCount(const url::Origin& origin) {
  auto it = origins_.find(origin);
  CHECK(it != origins_.end());
  auto& connection_count = it->second;
  CHECK_GT(connection_count, 0);

  connection_count--;
  total_connection_count_--;
  if (connection_count == 0) {
    origins_.erase(it);
  }

  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }
  if (total_connection_count_ == 0) {
    hid_system_tray_icon->UnstageProfile(profile_, /*immediate=*/false);
  } else {
    hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
  }
}

void HidConnectionTracker::ShowContentSettingsExceptions() {
  chrome::ShowContentSettingsExceptionsForProfile(
      profile_, ContentSettingsType::HID_CHOOSER_DATA);
}

void HidConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  chrome::ShowSiteSettings(profile_, origin.GetURL());
}

void HidConnectionTracker::CleanUp() {
  origins_.clear();
  total_connection_count_ = 0;
  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  // We can't rely on |origins_.empty()| to determine if the profile is in the
  // system tray icon, because |origins_| is updated immediately when a
  // profile is unstaged. In the scenario of |UnstageProfile(imm = false)|
  // followed by profile destruction, |UnstageProfile(imm = true)| is skipped
  // due to |origins_| being empty, and the |CleanUpProfile| callback 10s later
  // is a no-op because the profile is destroyed. Therefore, we need to
  // explicitly check if the profile is contained in the system tray icon.
  if (hid_system_tray_icon && hid_system_tray_icon->ContainProfile(profile_)) {
    hid_system_tray_icon->UnstageProfile(profile_, /*immediate=*/true);
  }
}
