// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_USER_MENU_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_USER_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_button.h"
#include "base/strings/string16.h"
#include "components/user_manager/user_type.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

class RemoveUserButton;

class ASH_EXPORT LoginUserMenuView : public LoginBaseBubbleView,
                                     public views::ButtonListener {
 public:
  class TestApi {
   public:
    explicit TestApi(LoginUserMenuView* bubble);
    views::View* remove_user_button();
    views::View* remove_user_confirm_data();
    views::Label* username_label();

   private:
    LoginUserMenuView* bubble_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  LoginUserMenuView(const base::string16& username,
                    const base::string16& email,
                    user_manager::UserType type,
                    bool is_owner,
                    views::View* anchor_view,
                    LoginButton* bubble_opener,
                    bool show_remove_user,
                    base::RepeatingClosure on_remove_user_warning_shown,
                    base::RepeatingClosure on_remove_user_requested);

  ~LoginUserMenuView() override;

  // Resets the user menu to the state where Remove User has not been pressed.
  void ResetState();

  // LoginBaseBubbleView:
  LoginButton* GetBubbleOpener() const override;
  gfx::Point CalculatePosition() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  void RequestFocus() override;
  bool HasFocus() const override;
  const char* GetClassName() const override;

 private:
  LoginButton* bubble_opener_ = nullptr;
  base::RepeatingClosure on_remove_user_warning_shown_;
  base::RepeatingClosure on_remove_user_requested_;
  views::View* remove_user_confirm_data_ = nullptr;
  views::Label* remove_user_label_ = nullptr;
  RemoveUserButton* remove_user_button_ = nullptr;
  views::Label* username_label_ = nullptr;

  base::string16 warning_message_;

  DISALLOW_COPY_AND_ASSIGN(LoginUserMenuView);
};

}  // namespace ash

#endif
