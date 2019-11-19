// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/login_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace ash {

class ArrowButtonView;
class LoginUserView;
class RightPaneView;
class PublicAccountWarningDialog;
struct LoginUserInfo;

// Implements an expanded view for the public account user to select language
// and keyboard options.
class ASH_EXPORT LoginExpandedPublicAccountView : public NonAccessibleView {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginExpandedPublicAccountView* view);
    ~TestApi();

    views::View* advanced_view_button();
    ArrowButtonView* submit_button();
    views::View* advanced_view();
    PublicAccountWarningDialog* warning_dialog();
    views::StyledLabel* learn_more_label();
    views::View* language_selection_button();
    views::View* keyboard_selection_button();
    LoginMenuView* language_menu_view();
    LoginMenuView* keyboard_menu_view();
    LoginMenuView::Item selected_language_item();
    LoginMenuView::Item selected_keyboard_item();
    views::ImageView* monitoring_warning_icon();
    views::Label* monitoring_warning_label();
    void ResetUserForTest();

   private:
    LoginExpandedPublicAccountView* const view_;
  };

  using OnPublicSessionViewDismissed = base::RepeatingClosure;
  explicit LoginExpandedPublicAccountView(
      const OnPublicSessionViewDismissed& on_dismissed);
  ~LoginExpandedPublicAccountView() override;

  void ProcessPressedEvent(const ui::LocatedEvent* event);
  void UpdateForUser(const LoginUserInfo& user);
  const LoginUserInfo& current_user() const;
  void Hide();
  void ShowWarningDialog();
  void OnWarningDialogClosed();
  void SetShowFullManagementDisclosure(bool show_full_management_disclosure);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  LoginUserView* user_view_ = nullptr;
  RightPaneView* right_pane_ = nullptr;
  OnPublicSessionViewDismissed on_dismissed_;
  PublicAccountWarningDialog* warning_dialog_ = nullptr;
  std::unique_ptr<ui::EventHandler> event_handler_;

  base::WeakPtrFactory<LoginExpandedPublicAccountView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginExpandedPublicAccountView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_
