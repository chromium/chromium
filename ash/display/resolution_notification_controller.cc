// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/screen_layout_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;

namespace ash {
namespace {

const char kNotifierDisplayResolutionChange[] = "ash.display.resolution-change";

bool g_use_timer = true;

}  // namespace

// static
const int ResolutionNotificationController::kTimeoutInSec = 15;

// static
const char ResolutionNotificationController::kNotificationId[] =
    "chrome://settings/display/resolution";

struct ResolutionNotificationController::ResolutionChangeInfo {
  ResolutionChangeInfo(int64_t display_id,
                       const display::ManagedDisplayMode& old_resolution,
                       const display::ManagedDisplayMode& new_resolution,
                       base::OnceClosure accept_callback);
  ~ResolutionChangeInfo();

  // The id of the display where the resolution change happens.
  const int64_t display_id;

  // The resolution before the change.
  display::ManagedDisplayMode old_resolution;

  // The requested resolution. Note that this may be different from
  // |current_resolution| which is the actual resolution set.
  display::ManagedDisplayMode new_resolution;

  // The actual resolution after the change.
  display::ManagedDisplayMode current_resolution;

  // The callback when accept is chosen.
  base::OnceClosure accept_callback;

  // The remaining timeout in seconds. 0 if the change does not time out.
  uint8_t timeout_count;

  // The timer to invoke OnTimerTick() every second. This cannot be
  // OneShotTimer since the message contains text "automatically closed in xx
  // seconds..." which has to be updated every second.
  base::RepeatingTimer timer;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolutionChangeInfo);
};

ResolutionNotificationController::ResolutionChangeInfo::ResolutionChangeInfo(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    base::OnceClosure accept_callback)
    : display_id(display_id),
      old_resolution(old_resolution),
      new_resolution(new_resolution),
      accept_callback(std::move(accept_callback)),
      timeout_count(0) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (!display::Display::HasInternalDisplay() &&
      display_manager->num_connected_displays() == 1u &&
      Shell::Get()->session_controller()->login_status() !=
          LoginStatus::KIOSK_APP) {
    // Introduce a timeout if we have a single external display and the device
    // is not in Kiosk mode. (The resolution change notification is invisible in
    // Kiosk mode, so do not introduce the timeout in this case.)
    timeout_count = kTimeoutInSec;
  }
}

ResolutionNotificationController::ResolutionChangeInfo::
    ~ResolutionChangeInfo() = default;

ResolutionNotificationController::ResolutionNotificationController() {
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

ResolutionNotificationController::~ResolutionNotificationController() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool ResolutionNotificationController::PrepareNotificationAndSetDisplayMode(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    ash::mojom::DisplayConfigSource source,
    base::OnceClosure accept_callback) {
  Shell::Get()->screen_layout_observer()->SetDisplayChangedFromSettingsUI(
      display_id);
  display::DisplayManager* const display_manager =
      Shell::Get()->display_manager();
  if (source == ash::mojom::DisplayConfigSource::kPolicy ||
      display::Display::IsInternalDisplayId(display_id)) {
    // We don't show notifications to confirm/revert the resolution change in
    // the case of an internal display or policy-forced changes.
    return display_manager->SetDisplayMode(display_id, new_resolution);
  }

  // If multiple resolution changes are invoked for the same display,
  // the original resolution for the first resolution change has to be used
  // instead of the specified |old_resolution|.
  display::ManagedDisplayMode original_resolution;
  if (change_info_ && change_info_->display_id == display_id) {
    DCHECK(change_info_->new_resolution.size() == old_resolution.size());
    original_resolution = change_info_->old_resolution;
  }

  if (change_info_ && change_info_->display_id != display_id) {
    // Preparing the notification for a new resolution change of another display
    // before the previous one was accepted. We decided that it's safer to
    // revert the previous resolution change since the user didn't explicitly
    // accept it, and we have no way of knowing for sure that it worked.
    RevertResolutionChange(false /* display_was_removed */);
  }

  change_info_ = std::make_unique<ResolutionChangeInfo>(
      display_id, old_resolution, new_resolution, std::move(accept_callback));
  if (!original_resolution.size().IsEmpty())
    change_info_->old_resolution = original_resolution;

  if (!display_manager->SetDisplayMode(display_id, new_resolution)) {
    // Discard the prepared notification data since we failed to set the new
    // resolution.
    change_info_.reset();
    return false;
  }

  return true;
}

bool ResolutionNotificationController::DoesNotificationTimeout() {
  return change_info_ && change_info_->timeout_count > 0;
}

void ResolutionNotificationController::Close(bool by_user) {
  if (by_user)
    AcceptResolutionChange(false);
}

void ResolutionNotificationController::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  // If there's the timeout, the first button is "Accept". Otherwise the
  // button click should be "Revert". Clicking on the body should accept.
  if (!button_index || (DoesNotificationTimeout() && *button_index == 0))
    AcceptResolutionChange(true);
  else
    RevertResolutionChange(false /* display_was_removed */);
}

void ResolutionNotificationController::CreateOrUpdateNotification(
    bool enable_spoken_feedback) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (!change_info_) {
    message_center->RemoveNotification(kNotificationId, false /* by_user */);
    return;
  }

  base::string16 timeout_message;
  message_center::RichNotificationData data;
  if (change_info_->timeout_count > 0) {
    data.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_RESOLUTION_CHANGE_ACCEPT)));
    timeout_message = l10n_util::GetStringFUTF16(
        IDS_ASH_DISPLAY_RESOLUTION_TIMEOUT,
        ui::TimeFormat::Simple(
            ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
            base::TimeDelta::FromSeconds(change_info_->timeout_count)));
  }
  data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_RESOLUTION_CHANGE_REVERT)));

  data.should_make_spoken_feedback_for_popup_updates = enable_spoken_feedback;

  const base::string16 display_name =
      base::UTF8ToUTF16(Shell::Get()->display_manager()->GetDisplayNameForId(
          change_info_->display_id));
  const base::string16 message =
      (change_info_->new_resolution.size() ==
       change_info_->current_resolution.size())
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED, display_name,
                base::UTF8ToUTF16(
                    change_info_->new_resolution.size().ToString()))
          : l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED_TO_UNSUPPORTED,
                display_name,
                base::UTF8ToUTF16(
                    change_info_->new_resolution.size().ToString()),
                base::UTF8ToUTF16(
                    change_info_->current_resolution.size().ToString()));

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, message,
      timeout_message,
      base::string16(),  // display_source
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierDisplayResolutionChange),
      data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()),
      kNotificationScreenIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);

  message_center->AddNotification(std::move(notification));
}

void ResolutionNotificationController::AcceptResolutionChange(
    bool close_notification) {
  if (close_notification) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
  }
  if (!change_info_)
    return;
  base::OnceClosure callback = std::move(change_info_->accept_callback);
  change_info_.reset();
  std::move(callback).Run();
}

void ResolutionNotificationController::RevertResolutionChange(
    bool display_was_removed) {
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           false /* by_user */);
  if (!change_info_)
    return;
  const int64_t display_id = change_info_->display_id;
  display::ManagedDisplayMode old_resolution = change_info_->old_resolution;
  change_info_.reset();
  Shell::Get()->screen_layout_observer()->SetDisplayChangedFromSettingsUI(
      display_id);
  if (display_was_removed) {
    // If display was removed then we are inside the stack of
    // DisplayManager::UpdateDisplaysWith(), and we need to update the selected
    // mode of this removed display without reentering again into
    // UpdateDisplaysWith() because this can cause a crash. crbug.com/709722.
    Shell::Get()->display_manager()->SetSelectedModeForDisplayId(
        display_id, old_resolution);
  } else {
    Shell::Get()->display_manager()->SetDisplayMode(display_id, old_resolution);
  }
}

void ResolutionNotificationController::OnTimerTick() {
  if (!change_info_)
    return;

  if (--change_info_->timeout_count == 0)
    RevertResolutionChange(false /* display_was_removed */);
  else
    CreateOrUpdateNotification(false);
}

void ResolutionNotificationController::OnDisplayRemoved(
    const display::Display& old_display) {
  if (change_info_ && change_info_->display_id == old_display.id())
    RevertResolutionChange(true /* display_was_removed */);
}

void ResolutionNotificationController::OnDisplayConfigurationChanged() {
  if (!change_info_)
    return;

  display::ManagedDisplayMode mode;
  if (Shell::Get()->display_manager()->GetActiveModeForDisplayId(
          change_info_->display_id, &mode)) {
    change_info_->current_resolution = mode;
  }

  CreateOrUpdateNotification(true);
  if (g_use_timer && change_info_->timeout_count > 0) {
    change_info_->timer.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                              &ResolutionNotificationController::OnTimerTick);
  }
}

void ResolutionNotificationController::SuppressTimerForTest() {
  g_use_timer = false;
}

}  // namespace ash
