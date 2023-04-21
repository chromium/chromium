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
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using base::TimeTicks;

std::string GetOriginName(Profile* profile, const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    const auto* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    CHECK(extension_registry);
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    // The extension must be installed if we are generating the name.
    CHECK(extension);
    return extension->name();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

}  // namespace

HidConnectionTracker::HidConnectionTracker(Profile* profile)
    : profile_(profile) {}

HidConnectionTracker::~HidConnectionTracker() {
  CleanUp();
}

void HidConnectionTracker::IncrementConnectionCount(const url::Origin& origin) {
  bool to_stage_profile = origins_.empty();
  auto& state = origins_[origin];

  CHECK_GE(state.count, 0);
  if (state.count == 0) {
    state.name = GetOriginName(profile_, origin);
  }
  state.count++;
  state.timestamp = TimeTicks::Now();
  total_connection_count_++;

  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }
  if (to_stage_profile) {
    hid_system_tray_icon->StageProfile(profile_);
  } else {
    hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
  }
}

void HidConnectionTracker::DecrementConnectionCount(const url::Origin& origin) {
  auto it = origins_.find(origin);
  CHECK(it != origins_.end());

  auto& state = it->second;
  CHECK_GT(state.count, 0);
  state.count--;
  state.timestamp = TimeTicks::Now();
  total_connection_count_--;
  if (state.count == 0) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&HidConnectionTracker::CleanUpOrigin,
                           weak_factory_.GetWeakPtr(), origin, state.timestamp),
            kOriginInactiveTime);
  }

  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }
  hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
}

void HidConnectionTracker::ShowContentSettingsExceptions() {
  chrome::ShowContentSettingsExceptionsForProfile(
      profile_, ContentSettingsType::HID_CHOOSER_DATA);
}

void HidConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  chrome::ShowSiteSettings(profile_, origin.GetURL());
}

void HidConnectionTracker::CleanUp() {
  if (!origins_.empty()) {
    origins_.clear();
    total_connection_count_ = 0;
    auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
    if (hid_system_tray_icon) {
      hid_system_tray_icon->UnstageProfile(profile_, /*immediate=*/true);
    }
  }
}

void HidConnectionTracker::CleanUpOrigin(const url::Origin& origin,
                                         const TimeTicks& timestamp) {
  auto it = origins_.find(origin);
  if (it == origins_.end()) {
    // This can happen if the connection bounces within 1 microsecond, which is
    // the base unit of base::TimeTicks. The first CleanUpOrigin call will clear
    // the origin because it sees the timestamp as the same.
    return;
  }
  auto& state = it->second;
  if (state.count == 0 && state.timestamp == timestamp) {
    origins_.erase(it);
    auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
    if (!hid_system_tray_icon) {
      return;
    }
    if (origins_.empty()) {
      hid_system_tray_icon->UnstageProfile(profile_, /*immediate=*/true);
    } else {
      hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
    }
  }
}
