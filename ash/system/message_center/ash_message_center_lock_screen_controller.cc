// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

// static private
base::Optional<AshMessageCenterLockScreenController::Mode>
    AshMessageCenterLockScreenController::overridden_mode_for_testing_;

// static
bool AshMessageCenterLockScreenController::IsEnabled() {
  auto mode = GetMode();
  bool is_showing = (mode == Mode::SHOW || mode == Mode::HIDE_SENSITIVE);
  // If |isAllowed()| is false, must return false;
  DCHECK(!is_showing || IsAllowed());
  return is_showing;
}

// static
bool AshMessageCenterLockScreenController::IsAllowed() {
  return GetMode() != Mode::PROHIBITED;
}

// static, private
AshMessageCenterLockScreenController::Mode
AshMessageCenterLockScreenController::GetMode() {
  if (overridden_mode_for_testing_.has_value())
    return *overridden_mode_for_testing_;

  if (!features::IsLockScreenNotificationsEnabled())
    return Mode::PROHIBITED;

  // User prefs may be null in some tests.
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!user_prefs)
    return Mode::PROHIBITED;

  const std::string& mode =
      user_prefs->GetString(prefs::kMessageCenterLockScreenMode);
  if (mode == prefs::kMessageCenterLockScreenModeShow)
    return Mode::SHOW;
  if (mode == prefs::kMessageCenterLockScreenModeHideSensitive &&
      features::IsLockScreenHideSensitiveNotificationsSupported())
    return Mode::HIDE_SENSITIVE;

  return Mode::HIDE;
}

// static, only for testing
void AshMessageCenterLockScreenController::OverrideModeForTest(
    base::Optional<AshMessageCenterLockScreenController::Mode> new_mode) {
  overridden_mode_for_testing_ = new_mode;
}

namespace {
const char kToastId[] = "ash-lock-screen-manager";
}  // anonymous namespace

AshMessageCenterLockScreenController::AshMessageCenterLockScreenController()
    : locked_(Shell::Get()->session_controller()->IsScreenLocked()) {}

AshMessageCenterLockScreenController::~AshMessageCenterLockScreenController() {
  // Invokes the cancel task if any.
  if (cancel_task_)
    std::move(cancel_task_).Run();
}

void AshMessageCenterLockScreenController::DismissLockScreenThenExecute(
    base::OnceClosure pending_callback,
    base::OnceClosure cancel_callback,
    int message_id) {
  if (locked_) {
    // Invokes the previous cancel task if any.
    if (cancel_task_)
      std::move(cancel_task_).Run();

    // Stores the new pending/cancel tasks.
    pending_task_ = std::move(pending_callback);
    cancel_task_ = std::move(cancel_callback);

    EncourageUserToUnlock(message_id);
  } else {
    DCHECK(pending_task_.is_null());
    DCHECK(cancel_task_.is_null());
    if (pending_callback)
      std::move(pending_callback).Run();
  }
}

bool AshMessageCenterLockScreenController::IsScreenLocked() const {
  return locked_;
}

void AshMessageCenterLockScreenController::EncourageUserToUnlock(
    int message_id) {
  DCHECK(locked_);

  DCHECK(LockScreen::Get());
  DCHECK(LockScreen::Get()->widget());
  auto* unified_system_tray =
      Shelf::ForWindow(LockScreen::Get()->widget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->unified_system_tray();
  if (unified_system_tray) {
    // Lockscreen notification works only with the unified system tray.
    unified_system_tray->CloseBubble();
  }

  base::string16 message;
  if (message_id != -1) {
    message = l10n_util::GetStringUTF16(message_id);
  } else {
    message =
        (Shell::Get()->session_controller()->NumberOfLoggedInUsers() == 1 ||
         active_account_id_.empty())
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_MESSAGE_CENTER_UNLOCK_TO_PERFORM_ACTION)
            : l10n_util::GetStringFUTF16(
                  IDS_ASH_MESSAGE_CENTER_UNLOCK_TO_PERFORM_ACTION_WITH_USER_ID,
                  base::UTF8ToUTF16(active_account_id_.GetUserEmail()));
  }

  // TODO(yoshiki): Update UI after the UX finalizes.
  Shell::Get()->toast_manager()->Show(
      ToastData(kToastId, message, ToastData::kInfiniteDuration, base::nullopt,
                /*visible_on_lock_screen=*/true));
}

void AshMessageCenterLockScreenController::OnLockStateChanged(bool locked) {
  if (locked_ == locked)
    return;

  locked_ = locked;

  if (!locked) {
    Shell::Get()->toast_manager()->Cancel(kToastId);

    // Invokes the pending task and resets the cancel task.
    if (pending_task_)
      std::move(pending_task_).Run();
    std::move(cancel_task_).Reset();
  } else {
    DCHECK(pending_task_.is_null());
    DCHECK(cancel_task_.is_null());
  }
}

void AshMessageCenterLockScreenController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  if (active_account_id_ == account_id)
    return;

  active_account_id_ = account_id;

  if (locked_) {
    // Cancels the current callbacks, if the user switches the active account
    // on the lock screen.
    DCHECK(Shell::Get());
    DCHECK(Shell::Get()->toast_manager());
    Shell::Get()->toast_manager()->Cancel(kToastId);

    std::move(pending_task_).Reset();
    if (cancel_task_)
      std::move(cancel_task_).Run();
  }
}

}  // namespace ash
