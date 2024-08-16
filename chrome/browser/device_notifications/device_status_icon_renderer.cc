// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_status_icon_renderer.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

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
  NOTREACHED();
}

}  // namespace

DeviceStatusIconRenderer::DeviceStatusIconRenderer(
    DeviceSystemTrayIcon* device_system_tray_icon,
    const chrome::HelpSource help_source,
    const int about_device_message_id)
    : DeviceSystemTrayIconRenderer(device_system_tray_icon),
      help_source_(help_source),
      about_device_message_id_(about_device_message_id) {}

DeviceStatusIconRenderer::~DeviceStatusIconRenderer() {
  if (status_icon_) {
    auto* status_tray = g_browser_process->status_tray();
    DCHECK(status_tray);
    status_tray->RemoveStatusIcon(status_icon_);
  }
}

std::u16string DeviceStatusIconRenderer::GetAboutDeviceLabel() {
  return l10n_util::GetStringUTF16(about_device_message_id_);
}

void DeviceStatusIconRenderer::AddProfile(Profile* profile) {
  if (device_system_tray_icon_->profiles().size() == 1) {
    auto* profile_manager = g_browser_process->profile_manager();
    CHECK(profile_manager);
    profile_manager->GetProfileAttributesStorage().AddObserver(this);
  }
  RefreshIcon();
}

void DeviceStatusIconRenderer::RemoveProfile(Profile* profile) {
  RefreshIcon();
  if (device_system_tray_icon_->profiles().empty()) {
    auto* profile_manager = g_browser_process->profile_manager();
    CHECK(profile_manager);
    profile_manager->GetProfileAttributesStorage().RemoveObserver(this);
  }
}

void DeviceStatusIconRenderer::NotifyConnectionUpdated(Profile* profile) {
  RefreshIcon();
}

void DeviceStatusIconRenderer::ExecuteCommand(int command_id, int event_flags) {
  CHECK_GE(command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST);
  CHECK_LE(command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_LAST);
  size_t command_idx = command_id - IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
  if (command_idx < command_id_callbacks_.size()) {
    command_id_callbacks_[command_idx].Run();
  }
}

void DeviceStatusIconRenderer::ShowHelpCenterUrl() {
  auto* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  CHECK(profile);
  chrome::ShowHelpForProfile(profile, help_source_);
}

void DeviceStatusIconRenderer::ShowContentSettings(
    base::WeakPtr<Profile> profile) {
  auto* connection_tracker =
      device_system_tray_icon_->GetConnectionTracker(profile);
  if (connection_tracker) {
    connection_tracker->ShowContentSettingsExceptions();
  }
}

void DeviceStatusIconRenderer::ShowSiteSettings(base::WeakPtr<Profile> profile,
                                                const url::Origin& origin) {
  auto* connection_tracker =
      device_system_tray_icon_->GetConnectionTracker(profile);
  if (connection_tracker) {
    connection_tracker->ShowSiteSettings(origin);
  }
}

void DeviceStatusIconRenderer::RefreshIcon() {
  command_id_callbacks_.clear();
  auto* status_tray = g_browser_process->status_tray();
  DCHECK(status_tray);
  if (device_system_tray_icon_->profiles().empty()) {
    if (status_icon_) {
      status_tray->RemoveStatusIcon(status_icon_);
      status_icon_ = nullptr;
    }
    return;
  }

  // This tries to construct this:
  // ------------------------------------------------
  // |Google Chrome is accessing Device device(s)      |
  // |About Device device                              |
  // |---------------Separator----------------------|
  // |Profile1 section (see below profile for loop) |
  // |...                                           |
  // |---------------Separator----------------------|
  // |ProfileN section                              |
  auto menu = std::make_unique<StatusIconMenuModel>(this);
  int total_connection_count = 0;
  int total_origin_count = 0;
  // Title will be updated after looping through profiles below.
#if !BUILDFLAG(IS_MAC)
  menu->AddTitle(u"");
#endif  //! BUILDFLAG(IS_MAC)
  AddItem(menu.get(), GetAboutDeviceLabel(),
          base::BindRepeating(&DeviceStatusIconRenderer::ShowHelpCenterUrl,
                              weak_factory_.GetWeakPtr()));
  for (const auto& [profile, staging] : device_system_tray_icon_->profiles()) {
    // Each profile section looks like this:
    // |---------------Separator---------------|
    // |profile name                           |
    // |Device settings                        |
    // |origin 1 is connecting to x device(s)  |
    // |...                                    |
    // |origin n is connecting to y device(s)  |
    menu->AddSeparator(ui::NORMAL_SEPARATOR);
    menu->AddTitle(GetProfileUserName(profile));
    AddItem(
        menu.get(), device_system_tray_icon_->GetContentSettingsLabel(),
        base::BindRepeating(&DeviceStatusIconRenderer::ShowContentSettings,
                            weak_factory_.GetWeakPtr(), profile->GetWeakPtr()));

    auto* connection_tracker =
        device_system_tray_icon_->GetConnectionTracker(profile->GetWeakPtr());
    CHECK(connection_tracker);
    total_origin_count += connection_tracker->origins().size();
    for (const auto& [origin, state] : connection_tracker->origins()) {
      AddItem(menu.get(),
              GetOriginConnectionCountLabel(profile, origin, state.count,
                                            state.name),
              base::BindRepeating(&DeviceStatusIconRenderer::ShowSiteSettings,
                                  weak_factory_.GetWeakPtr(),
                                  profile->GetWeakPtr(), origin));
      // Only consider the count that will be shown to the system tray icon, so
      // that connection count on title and extension buttons can match.
      total_connection_count += state.count;
    }
  }
  auto title_label = device_system_tray_icon_->GetTitleLabel(
      total_origin_count, total_connection_count);
#if !BUILDFLAG(IS_MAC)
  menu->SetLabel(0, title_label);
#endif  //! BUILDFLAG(IS_MAC)

  if (!status_icon_) {
    status_icon_ = status_tray->CreateStatusIcon(
        StatusTray::OTHER_ICON,
        gfx::CreateVectorIcon(device_system_tray_icon_->GetIcon(),
                              gfx::kGoogleGrey300),
        title_label);
  } else {
    status_icon_->SetToolTip(title_label);
  }
  status_icon_->SetContextMenu(std::move(menu));
}

void DeviceStatusIconRenderer::AddItem(StatusIconMenuModel* menu,
                                       std::u16string label,
                                       base::RepeatingClosure callback) {
  size_t index =
      command_id_callbacks_.size() + IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
  if (index > IDC_DEVICE_SYSTEM_TRAY_ICON_LAST) {
    // This case should be fairly rare, but if we have more items than
    // pre-defined command ids, we don't put those in the status icon menu.
    // TODO(crbug.com/40264386): Add a metric to capture this.
    return;
  }
  menu->AddItem(index, label);
  command_id_callbacks_.push_back(std::move(callback));
}

void DeviceStatusIconRenderer::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  auto* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (device_system_tray_icon_->profiles().contains(profile)) {
    NotifyConnectionUpdated(profile);
  }
}
