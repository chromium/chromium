// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_layout_observer.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/tablet_state.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

using message_center::Notification;

namespace ash {
namespace {

const char kNotifierDisplay[] = "ash.display";

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

std::u16string GetDisplayName(int64_t display_id) {
  return base::UTF8ToUTF16(
      GetDisplayManager()->GetDisplayNameForId(display_id));
}

std::u16string GetDisplaySize(int64_t display_id) {
  display::DisplayManager* display_manager = GetDisplayManager();

  // We don't show display size for mirrored display. Fallback
  // to empty string if this happens on release build.
  const display::DisplayIdList id_list =
      display_manager->GetMirroringDestinationDisplayIdList();
  const bool mirroring =
      display_manager->IsInMirrorMode() && base::Contains(id_list, display_id);
  DCHECK(!mirroring);
  if (mirroring)
    return std::u16string();

  const display::Display& display =
      display_manager->GetDisplayForId(display_id);
  DCHECK(display.is_valid());
  return base::UTF8ToUTF16(display.size().ToString());
}

// Callback to handle a user selecting the notification view.
void OnNotificationClicked(std::optional<int> button_index) {
  DCHECK(!button_index);

  // Settings may be blocked, e.g. at the lock screen.
  if (Shell::Get()->session_controller()->ShouldEnableSettings() &&
      Shell::Get()->system_tray_model()->client()) {
    Shell::Get()->system_tray_model()->client()->ShowDisplaySettings();
  }
  message_center::MessageCenter::Get()->RemoveNotification(
      ScreenLayoutObserver::kNotificationId, /*by_user=*/true);
}

// Returns the name of the currently connected external display whose ID is
// |external_display_id|. This should not be used when the external display is
// used for mirroring.
std::u16string GetExternalDisplayName(int64_t external_display_id) {
  DCHECK(!display::IsInternalDisplayId(external_display_id));

  display::DisplayManager* display_manager = GetDisplayManager();
  DCHECK(!display_manager->IsInMirrorMode());

  if (external_display_id == display::kInvalidDisplayId)
    return l10n_util::GetStringUTF16(IDS_DISPLAY_NAME_UNKNOWN);

  // The external display name may have an annotation of "(width x height)" in
  // case that the display is rotated or its resolution is changed.
  std::u16string name = GetDisplayName(external_display_id);
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(external_display_id);
  if (display_info.GetActiveRotation() != display::Display::ROTATE_0 ||
      !display_info.overscan_insets_in_dip().IsEmpty()) {
    name =
        l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME,
                                   name, GetDisplaySize(external_display_id));
  } else if (display_info.overscan_insets_in_dip().IsEmpty() &&
             display_info.has_overscan()) {
    name = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME, name,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN));
  }

  return name;
}

// Returns true if docked mode is currently enabled.
bool IsDockedModeEnabled() {
  display::DisplayManager* display_manager = GetDisplayManager();
  if (!display::HasInternalDisplay())
    return false;

  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    if (display::IsInternalDisplayId(display_manager->GetDisplayAt(i).id())) {
      return false;
    }
  }

  // We have an internal display but it's not one of the active displays.
  return true;
}

// Returns the notification message that should be shown when mirror display
// mode is entered.
std::u16string GetEnterMirrorModeMessage() {
  DCHECK(GetDisplayManager()->IsInMirrorMode());
  if (display::HasInternalDisplay()) {
    std::u16string display_names;
    for (auto& id :
         GetDisplayManager()->GetMirroringDestinationDisplayIdList()) {
      if (!display_names.empty())
        display_names.append(u",");
      display_names.append(GetDisplayName(id));
    }
    return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                      display_names);
  }

  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
}

// Returns the notification message that should be shown when unified desktop
// mode is entered.
std::u16string GetEnterUnifiedModeMessage() {
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED);
}

// Returns the notification message that should be shown when unified desktop
// mode is exited.
std::u16string GetExitUnifiedModeMessage() {
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED_EXITING);
}

std::u16string GetDisplayRemovedMessage(
    const display::ManagedDisplayInfo& removed_display_info,
    std::u16string* out_additional_message) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED,
      base::UTF8ToUTF16(removed_display_info.name()));
}

std::u16string GetDisplayAddedMessage(int64_t added_display_id,
                                      std::u16string* additional_message_out) {
  DCHECK(!display::IsInternalDisplayId(added_display_id));
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_ADDED,
                                    GetExternalDisplayName(added_display_id));
}

}  // namespace

const char ScreenLayoutObserver::kNotificationId[] =
    "chrome://settings/display";

ScreenLayoutObserver::ScreenLayoutObserver() {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
  UpdateDisplayInfo(nullptr);
}

ScreenLayoutObserver::~ScreenLayoutObserver() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
}

void ScreenLayoutObserver::SetDisplayChangedFromSettingsUI(int64_t display_id) {
  displays_changed_from_settings_ui_.insert(display_id);
}

void ScreenLayoutObserver::UpdateDisplayInfo(
    ScreenLayoutObserver::DisplayInfoMap* old_info) {
  if (old_info)
    old_info->swap(display_info_);
  display_info_.clear();

  display::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64_t id = display_manager->GetDisplayAt(i).id();
    display_info_[id] = display_manager->GetDisplayInfo(id);
  }
}

bool ScreenLayoutObserver::GetUnassociatedDisplayMessage(
    const ScreenLayoutObserver::DisplayInfoMap& old_info,
    std::u16string* out_message,
    std::u16string* out_additional_message) {
  if (old_display_mode_ != current_display_mode_) {
    // Ensure that user still gets notified of connecting with excessive
    // displays when display mode changes. For example, for the device which is
    // in tablet mode and screen layout is in extending mode, user connects one
    // additional external display to make the number of displays exceed the
    // maximum that device can support. Display mode changes from extending mode
    // to mirror mode.
    *out_additional_message = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM);

    // Detect changes in the mirror mode status.
    if (current_display_mode_ == DisplayMode::MIRRORING) {
      *out_message = GetEnterMirrorModeMessage();
      return true;
    }
    if (old_display_mode_ == DisplayMode::MIRRORING &&
        GetExitMirrorModeMessage(out_message, out_additional_message)) {
      return true;
    }

    // Detect changes in the unified mode status.
    if (current_display_mode_ == DisplayMode::UNIFIED) {
      *out_message = GetEnterUnifiedModeMessage();
      return true;
    }
    if (old_display_mode_ == DisplayMode::UNIFIED) {
      *out_message = GetExitUnifiedModeMessage();
      return true;
    }

    if (current_display_mode_ == DisplayMode::DOCKED ||
        old_display_mode_ == DisplayMode::DOCKED) {
      // We no longer show any notification for docked mode events.
      // crbug.com/674719.
      return false;
    }
  }

  // Displays are added or removed.
  if (display_info_.size() < old_info.size()) {
    // A display has been removed.
    for (const auto& iter : old_info) {
      if (display_info_.count(iter.first))
        continue;

      *out_message =
          GetDisplayRemovedMessage(iter.second, out_additional_message);
      return true;
    }
  }

  if (display_info_.size() > old_info.size()) {
    // A display has been added.
    for (const auto& iter : display_info_) {
      if (old_info.count(iter.first))
        continue;

      // No notification if the internal display is connected.
      if (display::IsInternalDisplayId(iter.first)) {
        return false;
      }

      *out_message = GetDisplayAddedMessage(iter.first, out_additional_message);
      return true;
    }
  }

  DCHECK_EQ(display_info_.size(), old_info.size());
  // When user connects more external display than the maximum that device
  // can support, |display_info_|'s size should be same with |old_info_|
  // because the displays which have unassociated crtc are not included in
  // |display_info_|.
  *out_additional_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM);
  return true;
}

void ScreenLayoutObserver::CreateOrUpdateNotification(
    const std::u16string& message,
    const std::u16string& additional_message) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);

  if (message.empty() && additional_message.empty())
    return;

  if (Shell::Get()
          ->screen_orientation_controller()
          ->ignore_display_configuration_updates()) {
    return;
  }

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, message,
      additional_message,
      std::u16string(),  // display_source
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierDisplay,
                                 NotificationCatalogName::kDisplayChange),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&OnNotificationClicked)),
      kNotificationScreenIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ScreenLayoutObserver::OnDidApplyDisplayChanges() {
  DisplayInfoMap old_info;
  UpdateDisplayInfo(&old_info);

  const bool current_has_unassociated_display =
      Shell::Get()->display_manager()->HasUnassociatedDisplay();

  // Take |has_unassociated_display_| into consideration in order to avoid
  // showing the notification too frequently. For example, user connects three
  // displays with device which supports at most two displays. Without checking
  // |has_unassociated_display_|, if user keeps three displays connected,
  // any event changing the display configuration would trigger the notification
  // of the unassociated display.
  const bool should_notify_has_unassociated_display =
      !has_unassociated_display_ && current_has_unassociated_display;

  has_unassociated_display_ = current_has_unassociated_display;

  old_display_mode_ = current_display_mode_;
  if (GetDisplayManager()->IsInMirrorMode())
    current_display_mode_ = DisplayMode::MIRRORING;
  else if (GetDisplayManager()->IsInUnifiedMode())
    current_display_mode_ = DisplayMode::UNIFIED;
  else if (IsDockedModeEnabled())
    current_display_mode_ = DisplayMode::DOCKED;
  else if (GetDisplayManager()->GetNumDisplays() > 2)
    current_display_mode_ = DisplayMode::EXTENDED_3_PLUS;
  else if (GetDisplayManager()->GetNumDisplays() == 2)
    current_display_mode_ = DisplayMode::EXTENDED_2;
  else
    current_display_mode_ = DisplayMode::SINGLE;

  // Only show notifications for unassociated displays.
  if (!should_notify_has_unassociated_display ||
      !show_notifications_for_testing_)
    return;

  std::u16string message;
  std::u16string additional_message;
  if (!GetUnassociatedDisplayMessage(old_info, &message, &additional_message)) {
    return;
  }

  CreateOrUpdateNotification(message, additional_message);
}

bool ScreenLayoutObserver::GetExitMirrorModeMessage(
    std::u16string* out_message,
    std::u16string* out_additional_message) {
  *out_message =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT);
  return true;
}

}  // namespace ash
