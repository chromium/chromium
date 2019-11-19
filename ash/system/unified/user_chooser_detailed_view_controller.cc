// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_detailed_view_controller.h"

#include "ash/multi_profile_uma.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_view.h"

namespace ash {

UserChooserDetailedViewController::UserChooserDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

UserChooserDetailedViewController::~UserChooserDetailedViewController() =
    default;

// static
bool UserChooserDetailedViewController::IsUserChooserEnabled() {
  // Don't allow user add or switch when CancelCastingDialog is open.
  // See http://crrev.com/291276 and http://crbug.com/353170.
  if (Shell::IsSystemModalWindowOpen())
    return false;

  // Don't allow at login, lock or when adding a multi-profile user.
  SessionControllerImpl* session = Shell::Get()->session_controller();
  if (session->IsUserSessionBlocked())
    return false;

  // Don't show if we cannot add or switch users.
  if (session->GetAddUserPolicy() != AddUserSessionPolicy::ALLOWED &&
      session->NumberOfLoggedInUsers() <= 1)
    return false;
  return true;
}

void UserChooserDetailedViewController::TransitionToMainView() {
  tray_controller_->TransitionToMainView(true /* restore_focus */);
}

void UserChooserDetailedViewController::HandleUserSwitch(int user_index) {
  // Do not switch users when the log screen is presented.
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  if (controller->IsUserSessionBlocked())
    return;

  // |user_index| must be in range (0, number_of_user). Note 0 is excluded
  // because it represents the active user and SwitchUser should not be called
  // for such case.
  DCHECK_GT(user_index, 0);
  DCHECK_LT(user_index, controller->NumberOfLoggedInUsers());

  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  tray_controller_->CloseBubble();
  controller->SwitchActiveUser(
      controller->GetUserSession(user_index)->user_info.account_id);
  // SwitchActiveUser may delete us.
}

void UserChooserDetailedViewController::HandleAddUserAction() {
  MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
  tray_controller_->CloseBubble();
  Shell::Get()->session_controller()->ShowMultiProfileLogin();
  // ShowMultiProfileLogin may delete us.
}

views::View* UserChooserDetailedViewController::CreateView() {
  return new UserChooserView(this);
}

}  // namespace ash
