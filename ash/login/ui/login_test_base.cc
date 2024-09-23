// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_base.h"

#include <string>

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "components/user_manager/known_user.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

LoginTestBase::LoginTestBase()
    : NoSessionAshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
  AuthEventsRecorder::Get()->OnAuthenticationSurfaceChange(
      AuthEventsRecorder::AuthenticationSurface::kLogin);
}

LoginTestBase::~LoginTestBase() = default;

void LoginTestBase::ShowLockScreen() {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  // The lock screen can't be shown without a wallpaper.
  Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
  Shell::Get()->login_screen_controller()->ShowLockScreen();
  // Allow focus to reach the appropriate View.
  base::RunLoop().RunUntilIdle();
}

void LoginTestBase::ShowLoginScreen(bool set_wallpaper) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // The login screen can't be shown without a wallpaper.
  if (set_wallpaper) {
    Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
  }

  Shell::Get()->login_screen_controller()->ShowLoginScreen();
  // Allow focus to reach the appropriate View.
  base::RunLoop().RunUntilIdle();
}

void LoginTestBase::SetWidget(std::unique_ptr<views::Widget> widget) {
  EXPECT_FALSE(widget_) << "SetWidget can only be called once.";
  widget_ = std::move(widget);
}

std::unique_ptr<views::Widget> LoginTestBase::CreateWidgetWithContent(
    views::View* content) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 800, 800);

  params.delegate = new views::WidgetDelegate();
  params.delegate->SetInitiallyFocusedView(content);
  params.delegate->SetOwnedByWidget(true);

  // Set the widget to the lock screen container, since a test may change the
  // session state to locked, which will hide all widgets not associated with
  // the lock screen.
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_LockScreenContainer);

  auto new_widget = std::make_unique<views::Widget>();
  new_widget->Init(std::move(params));
  new_widget->SetContentsView(content);
  new_widget->Show();
  return new_widget;
}

void LoginTestBase::SetUserCount(size_t count) {
  if (count > users_.size()) {
    AddUsers(count - users_.size());
    return;
  }

  users_.erase(users_.begin() + count, users_.end());
  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::AddUsers(size_t num_users) {
  for (size_t i = 0; i < num_users; i++) {
    std::string email = base::StrCat(
        {"user", base::NumberToString(users_.size()), "@domain.com"});
    users_.push_back(CreateUser(email));
  }

  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::AddUserByEmail(const std::string& email) {
  users_.push_back(CreateUser(email));
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::AddPublicAccountUsers(size_t num_public_accounts) {
  for (size_t i = 0; i < num_public_accounts; i++) {
    std::string email = base::StrCat(
        {"user", base::NumberToString(users_.size()), "@domain.com"});
    users_.push_back(CreatePublicAccountUser(email));
  }

  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::AddChildUsers(size_t num_users) {
  for (size_t i = 0; i < num_users; i++) {
    std::string email = base::StrCat(
        {"user", base::NumberToString(users_.size()), "@domain.com"});
    users_.push_back(CreateChildUser(email));
  }

  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::RemoveUser(const AccountId& account_id) {
  for (auto it = users().cbegin(); it != users().cend(); ++it) {
    if (it->basic_user_info.account_id == account_id) {
      users().erase(it);
      DataDispatcher()->SetUserList(users());
      return;
    }
  }
  ADD_FAILURE() << "User not found: " << account_id.Serialize();
}

LoginDataDispatcher* LoginTestBase::DataDispatcher() {
  return Shell::Get()->login_screen_controller()->data_dispatcher();
}

void LoginTestBase::TearDown() {
  if (LockScreen::HasInstance()) {
    LockScreen::Get()->Destroy();
  }

  widget_.reset();

  AshTestBase::TearDown();
}

}  // namespace ash
