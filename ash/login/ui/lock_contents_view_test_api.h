// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_TEST_API_H_
#define ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_TEST_API_H_

#include "ash/login/ui/kiosk_app_default_message.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen_media_controls_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_camera_timeout_view.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/login/ui/user_state.h"
#include "ash/public/cpp/login_types.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/views/view.h"

namespace ash {

// TestApi is used for tests to get internal implementation details.
class ASH_EXPORT LockContentsViewTestApi {
 public:
  explicit LockContentsViewTestApi(LockContentsView* view);
  ~LockContentsViewTestApi();

  KioskAppDefaultMessage* kiosk_default_message() const;
  LoginBigUserView* primary_big_view() const;
  LoginBigUserView* opt_secondary_big_view() const;
  AccountId focused_user() const;
  ScrollableUsersListView* users_list() const;
  LockScreenMediaControlsView* media_controls_view() const;
  views::View* note_action() const;
  views::View* tooltip_bubble() const;
  views::View* management_bubble() const;
  LoginErrorBubble* auth_error_bubble() const;
  LoginErrorBubble* detachable_base_error_bubble() const;
  LoginErrorBubble* warning_banner_bubble() const;
  views::View* user_adding_screen_indicator() const;
  views::View* system_info() const;
  views::View* bottom_status_indicator() const;
  BottomIndicatorState bottom_status_indicator_status() const;
  LoginExpandedPublicAccountView* expanded_view() const;
  views::View* main_view() const;
  const std::vector<UserState>& users() const;
  LoginCameraTimeoutView* login_camera_timeout_view() const;

  // Finds and focuses (if needed) Big User View view specified by
  // |account_id|. Returns nullptr if the user not found.
  LoginBigUserView* FindBigUser(const AccountId& account_id);
  LoginUserView* FindUserView(const AccountId& account_id);
  bool RemoveUser(const AccountId& account_id);
  bool IsOobeDialogVisible() const;
  FingerprintState GetFingerPrintState(const AccountId& account_id) const;

 private:
  const raw_ptr<LockContentsView, ExperimentalAsh> view_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_TEST_API_H_
