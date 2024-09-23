// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/resolution_notification_controller.h"

#include <utility>

#include "ash/display/display_change_dialog.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/screen_layout_observer.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/util/display_util.h"

namespace ash {

struct ResolutionNotificationController::ResolutionChangeInfo {
  ResolutionChangeInfo(int64_t display_id,
                       const display::ManagedDisplayMode& old_resolution,
                       const display::ManagedDisplayMode& new_resolution,
                       base::OnceClosure accept_callback);

  ResolutionChangeInfo(const ResolutionChangeInfo&) = delete;
  ResolutionChangeInfo& operator=(const ResolutionChangeInfo&) = delete;

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
};

ResolutionNotificationController::ResolutionChangeInfo::ResolutionChangeInfo(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    base::OnceClosure accept_callback)
    : display_id(display_id),
      old_resolution(old_resolution),
      new_resolution(new_resolution),
      accept_callback(std::move(accept_callback)) {}

ResolutionNotificationController::ResolutionChangeInfo::
    ~ResolutionChangeInfo() = default;

ResolutionNotificationController::ResolutionNotificationController() {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

ResolutionNotificationController::~ResolutionNotificationController() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
}

bool ResolutionNotificationController::PrepareNotificationAndSetDisplayMode(
    int64_t display_id,
    const display::ManagedDisplayMode& old_resolution,
    const display::ManagedDisplayMode& new_resolution,
    crosapi::mojom::DisplayConfigSource source,
    base::OnceClosure accept_callback) {
  Shell::Get()->screen_layout_observer()->SetDisplayChangedFromSettingsUI(
      display_id);
  display::DisplayManager* const display_manager =
      Shell::Get()->display_manager();
  if (source == crosapi::mojom::DisplayConfigSource::kPolicy ||
      display::IsInternalDisplayId(display_id)) {
    // We don't show notifications to confirm/revert the resolution change in
    // the case of an internal display or policy-forced changes.
    return display_manager->SetDisplayMode(display_id, new_resolution);
  }

  // If multiple resolution changes are invoked for the same display,
  // the original resolution for the first resolution change has to be used
  // instead of the specified |old_resolution|.
  display::ManagedDisplayMode original_resolution;
  if (change_info_ && change_info_->display_id == display_id) {
    DCHECK_EQ(change_info_->new_resolution.size(), old_resolution.size());
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
  if (!original_resolution.size().IsEmpty()) {
    change_info_->old_resolution = original_resolution;
  }

  if (!display_manager->SetDisplayMode(display_id, new_resolution)) {
    // Discard the prepared notification data since we failed to set the new
    // resolution.
    change_info_.reset();
    return false;
  }

  return true;
}

bool ResolutionNotificationController::ShouldShowDisplayChangeDialog() const {
  return change_info_ && Shell::Get()->session_controller()->login_status() !=
                             LoginStatus::KIOSK_APP;
}

void ResolutionNotificationController::CreateOrReplaceModalDialog() {
  if (confirmation_dialog_) {
    confirmation_dialog_->GetWidget()->CloseNow();
  }

  if (!ShouldShowDisplayChangeDialog()) {
    return;
  }

  const std::u16string display_name =
      base::UTF8ToUTF16(Shell::Get()->display_manager()->GetDisplayNameForId(
          change_info_->display_id));
  const std::u16string actual_display_size =
      base::UTF8ToUTF16(change_info_->current_resolution.size().ToString());
  const std::u16string requested_display_size =
      base::UTF8ToUTF16(change_info_->new_resolution.size().ToString());

  std::u16string dialog_title =
      l10n_util::GetStringUTF16(IDS_ASH_RESOLUTION_CHANGE_DIALOG_TITLE);

  // Construct the timeout message, leaving a placeholder for the countdown
  // timer so that the string does not need to be completely rebuilt every
  // timer tick.
  constexpr char16_t kTimeoutPlaceHolder[] = u"$1";

  std::u16string timeout_message_with_placeholder;
  if (display::features::IsListAllDisplayModesEnabled()) {
    const std::u16string actual_refresh_rate = ConvertRefreshRateToString16(
        change_info_->current_resolution.refresh_rate());
    const std::u16string requested_refresh_rate = ConvertRefreshRateToString16(
        change_info_->new_resolution.refresh_rate());

    const bool no_fallback = actual_display_size == requested_display_size &&
                             actual_refresh_rate == requested_refresh_rate;

    dialog_title =
        no_fallback
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_TITLE_SUCCESS)
            : l10n_util::GetStringUTF16(
                  IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_TITLE_FALLBACK);

    timeout_message_with_placeholder =
        no_fallback ? l10n_util::GetStringFUTF16(
                          IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_CHANGED_NEW,
                          display_name, actual_display_size,
                          actual_refresh_rate, kTimeoutPlaceHolder)
                    : l10n_util::GetStringFUTF16(
                          IDS_ASH_RESOLUTION_REFRESH_CHANGE_DIALOG_FALLBACK_NEW,
                          {display_name, actual_display_size,
                           actual_refresh_rate, requested_display_size,
                           requested_refresh_rate, kTimeoutPlaceHolder},
                          /*offsets=*/nullptr);

  } else {
    timeout_message_with_placeholder =
        actual_display_size == requested_display_size
            ? l10n_util::GetStringFUTF16(
                  IDS_ASH_RESOLUTION_CHANGE_DIALOG_CHANGED, display_name,
                  actual_display_size, kTimeoutPlaceHolder)
            : l10n_util::GetStringFUTF16(
                  IDS_ASH_RESOLUTION_CHANGE_DIALOG_FALLBACK, display_name,
                  requested_display_size, actual_display_size,
                  kTimeoutPlaceHolder);
  }

  DisplayChangeDialog* dialog = new DisplayChangeDialog(
      std::move(dialog_title), std::move(timeout_message_with_placeholder),
      base::BindOnce(&ResolutionNotificationController::AcceptResolutionChange,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ResolutionNotificationController::RevertResolutionChange,
                     weak_factory_.GetWeakPtr()));
  confirmation_dialog_ = dialog->GetWeakPtr();
}

void ResolutionNotificationController::AcceptResolutionChange() {
  if (!change_info_) {
    return;
  }
  base::OnceClosure callback = std::move(change_info_->accept_callback);
  change_info_.reset();
  std::move(callback).Run();
}

void ResolutionNotificationController::RevertResolutionChange(
    bool display_was_removed) {
  if (!change_info_) {
    return;
  }
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

void ResolutionNotificationController::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    if (change_info_ && change_info_->display_id == display.id()) {
      if (confirmation_dialog_) {
        // Use CloseWithReason rather than CloseNow to make sure the screen
        // doesn't stay dimmed after the widget is closed. b/288485093.
        confirmation_dialog_->GetWidget()->CloseWithReason(
            views::Widget::ClosedReason::kLostFocus);
      }
      RevertResolutionChange(/*display_was_removed=*/true);
      break;
    }
  }
}

void ResolutionNotificationController::OnDidApplyDisplayChanges() {
  if (!change_info_) {
    return;
  }

  display::ManagedDisplayMode mode;
  if (Shell::Get()->display_manager()->GetActiveModeForDisplayId(
          change_info_->display_id, &mode)) {
    change_info_->current_resolution = mode;
  }

  CreateOrReplaceModalDialog();
}

}  // namespace ash
