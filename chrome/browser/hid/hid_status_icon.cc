// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_status_icon.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

HidConnectionTracker* GetConnectionTracker(base::WeakPtr<Profile> profile) {
  if (!profile) {
    return nullptr;
  }
  return HidConnectionTrackerFactory::GetForProfile(profile.get(),
                                                    /*create=*/false);
}

// Returns profile username.
std::u16string GetProfileUserName(Profile* profile) {
  ProfileAttributesEntry* profile_attributes =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (profile_attributes) {
    return profile_attributes->GetName();
  }
  return base::UTF8ToUTF16(profile->GetProfileUserName());
}

// Returns a label for about HID device button.
std::u16string GetAboutDeviceLabel() {
  return l10n_util::GetStringUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_ABOUT_HID_DEVICE);
}

// Returns a label that displays the number of active connections for the
// specified origin.
std::u16string GetOriginConnectionCountLabel(Profile* profile,
                                             const url::Origin& origin,
                                             int connection_count,
                                             const std::string& name) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(IDS_DEVICE_CONNECTED_BY_EXTENSION),
        connection_count, base::UTF8ToUTF16(name));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

}  // namespace

HidStatusIcon::HidStatusIcon() = default;

HidStatusIcon::~HidStatusIcon() {
  if (status_icon_) {
    auto* status_tray = g_browser_process->status_tray();
    DCHECK(status_tray);
    status_tray->RemoveStatusIcon(status_icon_);
  }
}

void HidStatusIcon::ProfileAdded(Profile* profile) {
  if (profiles_.size() == 1) {
    auto* profile_manager = g_browser_process->profile_manager();
    CHECK(profile_manager);
    profile_manager->GetProfileAttributesStorage().AddObserver(this);
  }
  RefreshIcon();
}

void HidStatusIcon::ProfileRemoved(Profile* profile) {
  RefreshIcon();
  if (profiles_.empty()) {
    auto* profile_manager = g_browser_process->profile_manager();
    CHECK(profile_manager);
    profile_manager->GetProfileAttributesStorage().RemoveObserver(this);
  }
}

void HidStatusIcon::NotifyConnectionCountUpdated(Profile* profile) {
  RefreshIcon();
}

void HidStatusIcon::ExecuteCommand(int command_id, int event_flags) {
  CHECK_GE(command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST);
  CHECK_LE(command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_LAST);
  size_t command_idx = command_id - IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
  if (command_idx < command_id_callbacks_.size()) {
    command_id_callbacks_[command_idx].Run();
  }
}

// static
void HidStatusIcon::ShowHelpCenterUrl() {
  auto* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  CHECK(profile);
  chrome::ShowHelpForProfile(profile, chrome::HELP_SOURCE_WEBHID);
}

// static
void HidStatusIcon::ShowContentSettings(base::WeakPtr<Profile> profile) {
  auto* connection_tracker = GetConnectionTracker(profile);
  if (connection_tracker) {
    connection_tracker->ShowContentSettingsExceptions();
  }
}

// static
void HidStatusIcon::ShowSiteSettings(base::WeakPtr<Profile> profile,
                                     const url::Origin& origin) {
  auto* connection_tracker = GetConnectionTracker(profile);
  if (connection_tracker) {
    connection_tracker->ShowSiteSettings(origin);
  }
}

void HidStatusIcon::RefreshIcon() {
  command_id_callbacks_.clear();
  auto* status_tray = g_browser_process->status_tray();
  DCHECK(status_tray);
  if (profiles_.empty()) {
    if (status_icon_) {
      status_tray->RemoveStatusIcon(status_icon_);
      status_icon_ = nullptr;
    }
    return;
  }

  // This tries to construct this:
  // ------------------------------------------------
  // |Google Chrome is accessing HID device(s)      |
  // |About HID device                              |
  // |---------------Separator----------------------|
  // |Profile1 section (see below profile for loop) |
  // |...                                           |
  // |---------------Separator----------------------|
  // |ProfileN section                              |
  auto menu = std::make_unique<StatusIconMenuModel>(this);
  int total_connection_count = 0;
  int total_origin_count = 0;
  // Title will be updated after looping through profiles below.
  menu->AddTitle(u"");
  AddItem(menu.get(), GetAboutDeviceLabel(),
          base::BindRepeating(&HidStatusIcon::ShowHelpCenterUrl));
  for (const auto& [profile, staging] : profiles_) {
    // Each profile section looks like this:
    // |---------------Separator---------------|
    // |profile name                           |
    // |HID settings                           |
    // |origin 1 is connecting to x device(s)  |
    // |...                                    |
    // |origin n is connecting to y device(s)  |
    menu->AddSeparator(ui::NORMAL_SEPARATOR);
    menu->AddTitle(GetProfileUserName(profile));
    AddItem(menu.get(), GetContentSettingsLabel(),
            base::BindRepeating(&HidStatusIcon::ShowContentSettings,
                                profile->GetWeakPtr()));

    auto* connection_tracker =
        HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/false);
    CHECK(connection_tracker);
    total_origin_count += connection_tracker->origins().size();
    for (const auto& [origin, state] : connection_tracker->origins()) {
      AddItem(menu.get(),
              GetOriginConnectionCountLabel(profile, origin, state.count,
                                            state.name),
              base::BindRepeating(&HidStatusIcon::ShowSiteSettings,
                                  profile->GetWeakPtr(), origin));
      // Only consider the count that will be shown to the system tray icon, so
      // that connection count on title and extension buttons can match.
      total_connection_count += state.count;
    }
  }
  auto title_label = GetTitleLabel(total_origin_count, total_connection_count);
  menu->SetLabel(0, title_label);

  if (!status_icon_) {
    status_icon_ = status_tray->CreateStatusIcon(
        StatusTray::OTHER_ICON, GetStatusTrayIcon(), title_label);
  } else {
    status_icon_->SetToolTip(title_label);
  }
  status_icon_->SetContextMenu(std::move(menu));
}

void HidStatusIcon::AddItem(StatusIconMenuModel* menu,
                            std::u16string label,
                            base::RepeatingClosure callback) {
  size_t index =
      command_id_callbacks_.size() + IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
  if (index > IDC_DEVICE_SYSTEM_TRAY_ICON_LAST) {
    // This case should be fairly rare, but if we have more items than
    // pre-defined command ids, we don't put those in the status icon menu.
    // TODO(crbug.com/1433378): Add a metric to capture this.
    return;
  }
  menu->AddItem(index, label);
  command_id_callbacks_.push_back(std::move(callback));
}

void HidStatusIcon::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  auto* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (profiles_.contains(profile)) {
    NotifyConnectionCountUpdated(profile);
  }
}
