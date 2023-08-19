// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_TEST_BASE_H_
#define ASH_LOGIN_UI_LOGIN_TEST_BASE_H_

#include <memory>

#include "ash/public/cpp/login_types.h"
#include "ash/test/ash_test_base.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class AuthEventsRecorder;
class LoginDataDispatcher;

// Base test fixture for testing the views-based login and lock screens. This
// class provides easy access to types which the login/lock frequently need.
class LoginTestBase : public NoSessionAshTestBase {
 public:
  LoginTestBase();

  LoginTestBase(const LoginTestBase&) = delete;
  LoginTestBase& operator=(const LoginTestBase&) = delete;

  ~LoginTestBase() override;

  // Shows a full Lock/Login screen. These methods are useful for when we want
  // to test interactions between multiple lock screen components, or when some
  // component needs to be able to talk directly to the lockscreen (e.g. getting
  // the ScreenType).
  void ShowLockScreen();
  // If `set_wallpaper` is true, sets a wallpaper in the default color.
  void ShowLoginScreen(bool set_wallpaper = true);

  // Sets the primary test widget. The widget can be retrieved using |widget()|.
  // This can be used to make a widget scoped to the whole test, e.g. if the
  // widget is created in a SetUp override.
  // May be called at most once.
  void SetWidget(std::unique_ptr<views::Widget> widget);
  views::Widget* widget() const { return widget_.get(); }

  // Creates a widget containing |content|. The created widget will initially be
  // shown.
  std::unique_ptr<views::Widget> CreateWidgetWithContent(views::View* content);

  // Changes the active number of users. Fires an event on |DataDispatcher()|.
  void SetUserCount(size_t count);

  // Append number of |num_users| regular auth users.
  // Changes the active number of users. Fires an event on
  // |DataDispatcher()|.
  void AddUsers(size_t num_users);

  // Add a single user with the specified |email|.
  void AddUserByEmail(const std::string& email);

  // Append number of |num_public_accounts| public account users.
  // Changes the active number of users. Fires an event on
  // |DataDispatcher()|.
  void AddPublicAccountUsers(size_t num_public_accounts);

  // Creates and appends |num_users| of child user accounts.
  // Changes the active number of users. Fires an event on |DataDispatcher()|.
  void AddChildUsers(size_t num_users);

  // Removes user specified by |account_id| from users(). Fires an event on
  // |DataDispatcher()|
  void RemoveUser(const AccountId& account_id);

  std::vector<LoginUserInfo>& users() { return users_; }

  const std::vector<LoginUserInfo>& users() const { return users_; }

  // Returns the singleton LoginDataDispatcher.
  LoginDataDispatcher* DataDispatcher();

  // AshTestBase:
  void TearDown() override;

 private:
  // The widget set using `SetWidget`.
  std::unique_ptr<views::Widget> widget_;

  std::vector<LoginUserInfo> users_;
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_TEST_BASE_H_
