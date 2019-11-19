// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A WidgetDelegate which ensures that |initially_focused| gets focus.
class LoginTestBase::WidgetDelegate : public views::WidgetDelegate {
 public:
  explicit WidgetDelegate(views::View* content) : content_(content) {}
  ~WidgetDelegate() override = default;

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::View* GetInitiallyFocusedView() override { return content_; }
  views::Widget* GetWidget() override { return content_->GetWidget(); }
  const views::Widget* GetWidget() const override {
    return content_->GetWidget();
  }

 private:
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDelegate);
};

LoginTestBase::LoginTestBase() = default;

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

void LoginTestBase::ShowLoginScreen() {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // The login screen can't be shown without a wallpaper.
  Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
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
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 800, 800);
  params.delegate = new WidgetDelegate(content);

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
    std::string email =
        base::StrCat({"user", std::to_string(users_.size()), "@domain.com"});
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
    std::string email =
        base::StrCat({"user", std::to_string(users_.size()), "@domain.com"});
    users_.push_back(CreatePublicAccountUser(email));
  }

  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

void LoginTestBase::AddChildUsers(size_t num_users) {
  for (size_t i = 0; i < num_users; i++) {
    std::string email =
        base::StrCat({"user", std::to_string(users_.size()), "@domain.com"});
    users_.push_back(CreateChildUser(email));
  }

  // Notify any listeners that the user count has changed.
  DataDispatcher()->SetUserList(users_);
}

LoginDataDispatcher* LoginTestBase::DataDispatcher() {
  return Shell::Get()->login_screen_controller()->data_dispatcher();
}

void LoginTestBase::TearDown() {
  widget_.reset();

  if (LockScreen::HasInstance())
    LockScreen::Get()->Destroy();

  AshTestBase::TearDown();
}

}  // namespace ash
