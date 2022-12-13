// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_status_icon.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_vector.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "extensions/buildflags/buildflags.h"

HidStatusIcon::HidStatusIcon() = default;

HidStatusIcon::~HidStatusIcon() {
  if (status_icon_) {
    auto* status_tray = g_browser_process->status_tray();
    DCHECK(status_tray);
    status_tray->RemoveStatusIcon(status_icon_);
  }
}

void HidStatusIcon::AddProfile(Profile* profile) {
  DCHECK(!base::Contains(profiles_, profile));
  profiles_.emplace_back(profile);
  RefreshIcon();
}

void HidStatusIcon::RemoveProfile(Profile* profile) {
  DCHECK(base::Contains(profiles_, profile));
  base::EraseIf(profiles_, [&](Profile* entry) { return profile == entry; });
  RefreshIcon();
}

void HidStatusIcon::NotifyConnectionCountUpdated(Profile* profile) {
  DCHECK(status_icon_);
  DCHECK(base::Contains(profiles_, profile));
  status_icon_->SetToolTip(GetTooltipLabel(GetTotalConnectionCount()));
}

void HidStatusIcon::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_GE(command_id, IDC_MANAGE_HID_DEVICES_FIRST);
  DCHECK_LE(command_id, IDC_MANAGE_HID_DEVICES_LAST);
  size_t profile_idx = command_id - IDC_MANAGE_HID_DEVICES_FIRST;
  if (profile_idx < profiles_.size()) {
    // |profiles_[profile_idx]|'s HidConnectionTracker guarantees the entry in
    // |profiles_| is removed when the profile is destroyed.
    auto* hid_connection_tracker = HidConnectionTrackerFactory::GetForProfile(
        profiles_[profile_idx], /*create=*/false);
    DCHECK(hid_connection_tracker);
    hid_connection_tracker->ShowHidContentSettingsExceptions();
  }
}

size_t HidStatusIcon::GetTotalConnectionCount() {
  size_t total_connection_count = 0;
  for (Profile* profile : profiles_) {
    auto* hid_connection_tracker =
        HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/false);
    DCHECK(hid_connection_tracker);
    total_connection_count += hid_connection_tracker->connection_count();
  }
  return total_connection_count;
}

void HidStatusIcon::RefreshIcon() {
  auto* status_tray = g_browser_process->status_tray();
  DCHECK(status_tray);
  if (profiles_.empty()) {
    DCHECK(status_icon_);
    status_tray->RemoveStatusIcon(status_icon_);
    status_icon_ = nullptr;
    return;
  }

  auto menu = std::make_unique<StatusIconMenuModel>(this);
  for (size_t idx = 0; idx < profiles_.size(); idx++) {
    int command_id = IDC_MANAGE_HID_DEVICES_FIRST + static_cast<int>(idx);
    if (command_id > IDC_MANAGE_HID_DEVICES_LAST) {
      // This case should be fairly rare, but if we have more profiles than
      // pre-defined command ids, we don't put those in the status icon menu.
      // TODO(crbug.com/1360981): Add a metric to capture this.
      break;
    }
    menu->AddItem(command_id, GetManageHidDeviceButtonLabel(profiles_[idx]));
  }
  auto tooltip_label = GetTooltipLabel(GetTotalConnectionCount());

  if (!status_icon_) {
    status_icon_ = status_tray->CreateStatusIcon(
        StatusTray::OTHER_ICON, GetStatusTrayIcon(), tooltip_label);
  } else {
    status_icon_->SetToolTip(tooltip_label);
  }
  status_icon_->SetContextMenu(std::move(menu));
}
