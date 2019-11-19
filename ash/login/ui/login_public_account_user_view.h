// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PUBLIC_ACCOUNT_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PUBLIC_ACCOUNT_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class ArrowButtonView;
class HoverNotifier;

// This is the big user view for the public account user. It wraps a UserView
// and a arrow button below.
class ASH_EXPORT LoginPublicAccountUserView : public NonAccessibleView,
                                              public views::ButtonListener {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPublicAccountUserView* view);
    ~TestApi();

    views::View* arrow_button() const;

   private:
    LoginPublicAccountUserView* const view_;
  };

  using OnPublicAccountTapped = base::RepeatingClosure;

  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Called when the user taps the user view.
    LoginUserView::OnTap on_tap;
    // Called when the user taps on the public account user view.
    OnPublicAccountTapped on_public_account_tapped;
  };

  LoginPublicAccountUserView(const LoginUserInfo& user,
                             const Callbacks& callbacks);
  ~LoginPublicAccountUserView() override;

  void SetAuthEnabled(bool enabled, bool animate);
  void UpdateForUser(const LoginUserInfo& user);
  const LoginUserInfo& current_user() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  bool auth_enabled() const { return auth_enabled_; }
  LoginUserView* user_view() { return user_view_; }

 private:
  // Called when the user view has been tapped and it will run |on_tap_|.
  void OnUserViewTap();

  // Show an arrow button for public session when hovered.
  void OnHover(bool has_hover);

  // Update the opacity of the arrow button.
  void UpdateArrowButtonOpacity(float target_opacity, bool animate);

  const LoginUserView::OnTap on_tap_;
  const OnPublicAccountTapped on_public_account_tap_;

  // Used to show an arrow button for public session when hovered.
  std::unique_ptr<HoverNotifier> hover_notifier_;

  ArrowButtonView* arrow_button_ = nullptr;
  bool ignore_hover_ = false;
  bool auth_enabled_ = false;
  LoginUserView* user_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginPublicAccountUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PUBLIC_ACCOUNT_USER_VIEW_H_
