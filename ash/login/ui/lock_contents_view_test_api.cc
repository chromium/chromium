// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view_test_api.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/note_action_launch_button.h"

namespace ash {

LockContentsViewTestApi::LockContentsViewTestApi(LockContentsView* view)
    : view_(view) {}

LockContentsViewTestApi::~LockContentsViewTestApi() = default;

KioskAppDefaultMessage* LockContentsViewTestApi::kiosk_default_message() const {
  return view_->kiosk_default_message_;
}

LoginBigUserView* LockContentsViewTestApi::primary_big_view() const {
  return view_->primary_big_view_;
}

LoginBigUserView* LockContentsViewTestApi::opt_secondary_big_view() const {
  return view_->opt_secondary_big_view_;
}

AccountId LockContentsViewTestApi::focused_user() const {
  if (view_->CurrentBigUserView()->public_account()) {
    return view_->CurrentBigUserView()
        ->public_account()
        ->current_user()
        .basic_user_info.account_id;
  }
  return view_->CurrentBigUserView()
      ->auth_user()
      ->current_user()
      .basic_user_info.account_id;
}

ScrollableUsersListView* LockContentsViewTestApi::users_list() const {
  return view_->users_list_;
}

LockScreenMediaControlsView* LockContentsViewTestApi::media_controls_view()
    const {
  return view_->media_controls_view_;
}

LockScreenMediaView* LockContentsViewTestApi::media_view() const {
  return view_->media_view_;
}

views::View* LockContentsViewTestApi::note_action() const {
  return view_->note_action_;
}

views::View* LockContentsViewTestApi::management_bubble() const {
  return view_->management_bubble_;
}

LoginErrorBubble* LockContentsViewTestApi::auth_error_bubble() const {
  return view_->auth_error_bubble_;
}

LoginErrorBubble* LockContentsViewTestApi::detachable_base_error_bubble()
    const {
  return view_->detachable_base_error_bubble_;
}

LoginErrorBubble* LockContentsViewTestApi::warning_banner_bubble() const {
  return view_->warning_banner_bubble_;
}

views::View* LockContentsViewTestApi::user_adding_screen_indicator() const {
  return view_->user_adding_screen_indicator_;
}

views::View* LockContentsViewTestApi::system_info() const {
  return view_->system_info_;
}

views::View* LockContentsViewTestApi::bottom_status_indicator() const {
  return view_->bottom_status_indicator_;
}

BottomIndicatorState LockContentsViewTestApi::bottom_status_indicator_status()
    const {
  return view_->bottom_status_indicator_state_;
}

LoginExpandedPublicAccountView* LockContentsViewTestApi::expanded_view() const {
  return view_->expanded_view_;
}

views::View* LockContentsViewTestApi::main_view() const {
  return view_->main_view_;
}

const std::vector<UserState>& LockContentsViewTestApi::users() const {
  return view_->users_;
}

LoginCameraTimeoutView* LockContentsViewTestApi::login_camera_timeout_view()
    const {
  return view_->login_camera_timeout_view_;
}

LoginBigUserView* LockContentsViewTestApi::FindBigUser(
    const AccountId& account_id) {
  LoginBigUserView* big_view =
      view_->TryToFindBigUser(account_id, false /*require_auth_active*/);
  if (big_view) {
    return big_view;
  }
  LoginUserView* user_view = view_->TryToFindUserView(account_id);
  if (!user_view) {
    DLOG(ERROR) << "Could not find user: " << account_id.Serialize();
    return nullptr;
  }
  LoginUserView::TestApi user_view_api(user_view);
  user_view_api.OnTap();
  return view_->TryToFindBigUser(account_id, false /*require_auth_active*/);
}

LoginUserView* LockContentsViewTestApi::FindUserView(
    const AccountId& account_id) {
  if (view_->expanded_view_ && view_->expanded_view_->GetVisible()) {
    LoginExpandedPublicAccountView::TestApi expanded_test(
        view_->expanded_view_);
    return expanded_test.user_view();
  }
  return view_->TryToFindUserView(account_id);
}

bool LockContentsViewTestApi::RemoveUser(const AccountId& account_id) {
  LoginBigUserView* big_view = FindBigUser(account_id);
  if (!big_view) {
    return false;
  }
  if (!big_view->GetCurrentUser().can_remove) {
    return false;
  }
  LoginBigUserView::TestApi user_api(big_view);
  user_api.Remove();
  return true;
}

bool LockContentsViewTestApi::IsOobeDialogVisible() const {
  return view_->oobe_dialog_visible_;
}

FingerprintState LockContentsViewTestApi::GetFingerPrintState(
    const AccountId& account_id) const {
  UserState* user_state = view_->FindStateForUser(account_id);
  DCHECK(user_state);
  return user_state->fingerprint_state;
}

}  // namespace ash
