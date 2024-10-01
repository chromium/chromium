// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view_test_api.h"

#include <vector>

#include "ash/login/ui/auth_error_bubble.h"
#include "ash/login/ui/kiosk_app_default_message.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen_media_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/note_action_launch_button.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/public/cpp/login_types.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/views/view.h"

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

LockScreenMediaView* LockContentsViewTestApi::media_view() const {
  return view_->media_view_;
}

views::View* LockContentsViewTestApi::note_action() const {
  return view_->note_action_;
}

views::View* LockContentsViewTestApi::management_bubble() const {
  return view_->management_bubble_;
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

base::WeakPtr<ManagementDisclosureDialog>
LockContentsViewTestApi::management_disclosure_dialog() const {
  return view_->management_disclosure_dialog_;
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

AuthErrorBubble* LockContentsViewTestApi::auth_error_bubble() const {
  return view_->auth_error_bubble_;
}

bool LockContentsViewTestApi::IsAuthErrorBubbleVisible() const {
  return auth_error_bubble()->GetVisible();
}

void LockContentsViewTestApi::ShowAuthErrorBubble(int unlock_attempt) const {
  LoginBigUserView* big_view = view_->CurrentBigUserView();
  if (!big_view->auth_user()) {
    return;
  }

  const AccountId account_id =
      big_view->GetCurrentUser().basic_user_info.account_id;
  UserState* user_state = view_->FindStateForUser(account_id);

  auth_error_bubble()->ShowAuthError(
      /*anchor_view = */ big_view->auth_user()->GetActiveInputView(),
      /*unlock_attempt = */ unlock_attempt,
      /*show_pin = */ user_state->show_pin,
      /*is_login_screen = */ view_->screen_type_ ==
          LockScreen::ScreenType::kLogin);
}

void LockContentsViewTestApi::HideAuthErrorBubble() const {
  CHECK(IsAuthErrorBubbleVisible());
  auth_error_bubble()->Hide();
}

void LockContentsViewTestApi::PressAuthErrorRecoveryButton() const {
  CHECK(IsAuthErrorBubbleVisible());
  auth_error_bubble()->OnRecoverButtonPressed();
}

void LockContentsViewTestApi::PressAuthErrorLearnMoreButton() const {
  CHECK(IsAuthErrorBubbleVisible());
  auth_error_bubble()->OnLearnMoreButtonPressed();
}

void LockContentsViewTestApi::ToggleManagementForUser(const AccountId& user) {
  auto replace = [](const LoginUserInfo& user_info) {
    auto changed = user_info;
    if (user_info.user_account_manager) {
      changed.user_account_manager.reset();
    } else {
      changed.user_account_manager = "example@example.com";
    }
    return changed;
  };

  LoginBigUserView* big =
      view_->TryToFindBigUser(user, false /*require_auth_active*/);
  if (big) {
    big->UpdateForUser(replace(big->GetCurrentUser()));
    return;
  }

  LoginUserView* user_view =
      view_->users_list_ ? view_->users_list_->GetUserView(user) : nullptr;
  if (user_view) {
    user_view->UpdateForUser(replace(user_view->current_user()),
                             false /*animate*/);
    return;
  }
}

void LockContentsViewTestApi::SetMultiUserSignInPolicyForUser(
    const AccountId& user,
    user_manager::MultiUserSignInPolicy policy) {
  auto replace = [policy](const LoginUserInfo& user_info) {
    auto changed = user_info;
    changed.multi_user_sign_in_policy = policy;
    changed.is_multi_user_sign_in_allowed =
        policy == user_manager::MultiUserSignInPolicy::kUnrestricted;
    return changed;
  };

  LoginBigUserView* big =
      view_->TryToFindBigUser(user, false /*require_auth_active*/);
  if (big) {
    big->UpdateForUser(replace(big->GetCurrentUser()));
  }

  LoginUserView* user_view =
      view_->users_list_ ? view_->users_list_->GetUserView(user) : nullptr;
  if (user_view) {
    user_view->UpdateForUser(replace(user_view->current_user()),
                             false /*animate*/);
  }

  view_->LayoutAuth(view_->CurrentBigUserView(), nullptr /*opt_to_hide*/,
                    true /*animate*/);
}

void LockContentsViewTestApi::ToggleForceOnlineSignInForUser(
    const AccountId& user) {
  UserState* state = view_->FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user forcing online sign in";
    return;
  }
  state->force_online_sign_in = !state->force_online_sign_in;

  LoginBigUserView* big_user =
      view_->TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    view_->LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsViewTestApi::ToggleDisableTpmForUser(const AccountId& user) {
  UserState* state = view_->FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user to toggle TPM disabled message";
    return;
  }
  if (state->time_until_tpm_unlock.has_value()) {
    state->time_until_tpm_unlock = std::nullopt;
  } else {
    state->time_until_tpm_unlock = base::Minutes(5);
  }

  LoginBigUserView* big_user =
      view_->TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    view_->LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsViewTestApi::UndoForceOnlineSignInForUser(
    const AccountId& user) {
  UserState* state = view_->FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user forcing online sign in";
    return;
  }
  state->force_online_sign_in = false;

  LoginBigUserView* big_user =
      view_->TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    view_->LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsViewTestApi::SetKioskLicenseMode(bool is_kiosk_license_mode) {
  view_->kiosk_license_mode_ = is_kiosk_license_mode;

  // Normally when management device mode is updated, via
  // OnDeviceEnterpriseInfoChanged, it updates the visibility of Kiosk default
  // meesage too.
  view_->UpdateKioskDefaultMessageVisibility();
}

}  // namespace ash
