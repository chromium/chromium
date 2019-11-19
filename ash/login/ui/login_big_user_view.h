// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BIG_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_BIG_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_public_account_user_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/public/cpp/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"

namespace ash {

// Displays the big user view in the login screen. This is a container view
// which has one of the following views as its only child:
//  - LoginAuthUserView: for regular user.
//  - LoginPublicAccountUserView: for public account user.
class ASH_EXPORT LoginBigUserView : public NonAccessibleView,
                                    public WallpaperControllerObserver {
 public:
  LoginBigUserView(
      const LoginUserInfo& user,
      const LoginAuthUserView::Callbacks& auth_user_callbacks,
      const LoginPublicAccountUserView::Callbacks& public_account_callbacks);
  ~LoginBigUserView() override;

  // Base on the user type, call CreateAuthUser or CreatePublicAccount.
  void CreateChildView(const LoginUserInfo& user);

  // Update the displayed name, icon, etc to that of |user|.
  void UpdateForUser(const LoginUserInfo& user);

  // Safe to call in any state.
  const LoginUserInfo& GetCurrentUser() const;

  // Safe to call in any state.
  LoginUserView* GetUserView();

  // Safe to call in any state.
  bool IsAuthEnabled() const;

  LoginPublicAccountUserView* public_account() { return public_account_; }
  LoginAuthUserView* auth_user() { return auth_user_; }

  // views::View:
  void RequestFocus() override;

  // WallpaperControllerObserver:
  void OnWallpaperBlurChanged() override;

 private:
  // Create LoginAuthUserView and add it as child view.
  // |public_account_| will be deleted if exists to ensure the single child.
  void CreateAuthUser(const LoginUserInfo& user);

  // Create LoginPublicAccountUserView and add it as child view.
  // |auth_user_| will be deleted if exists to ensure the
  // single child.
  void CreatePublicAccount(const LoginUserInfo& user);

  // Either |auth_user_| or |public_account_| must be null.
  LoginPublicAccountUserView* public_account_ = nullptr;
  LoginAuthUserView* auth_user_ = nullptr;

  LoginAuthUserView::Callbacks auth_user_callbacks_;
  LoginPublicAccountUserView::Callbacks public_account_callbacks_;

  ScopedObserver<WallpaperController, WallpaperControllerObserver> observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(LoginBigUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BIG_USER_VIEW_H_
