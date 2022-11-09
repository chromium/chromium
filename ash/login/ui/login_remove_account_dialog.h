// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_REMOVE_ACCOUNT_DIALOG_H_
#define ASH_LOGIN_UI_LOGIN_REMOVE_ACCOUNT_DIALOG_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_button.h"
#include "components/user_manager/user_type.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"

namespace ash {

struct LoginUserInfo;
class RemoveUserButton;

class ASH_EXPORT LoginRemoveAccountDialog : public LoginBaseBubbleView,
                                            public views::FocusTraversable {
 public:
  class TestApi {
   public:
    explicit TestApi(LoginRemoveAccountDialog* bubble);
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;
    ~TestApi() = default;

    views::View* remove_user_button();
    views::View* remove_user_confirm_data();
    views::View* managed_user_data();
    views::Label* username_label();
    views::Label* management_disclosure_label();

   private:
    LoginRemoveAccountDialog* bubble_;
  };

  LoginRemoveAccountDialog(const LoginUserInfo& user,
                           base::WeakPtr<views::View> anchor_view,
                           LoginButton* bubble_opener,
                           base::RepeatingClosure on_remove_user_warning_shown,
                           base::RepeatingClosure on_remove_user_requested);
  LoginRemoveAccountDialog(const LoginRemoveAccountDialog&) = delete;
  LoginRemoveAccountDialog& operator=(const LoginRemoveAccountDialog&) = delete;
  ~LoginRemoveAccountDialog() override;

  // Resets the user menu to the state where Remove User has not been pressed.
  void ResetState();

  // LoginBaseBubbleView:
  LoginButton* GetBubbleOpener() const override;
  void OnThemeChanged() override;

  // views::View:
  void RequestFocus() override;
  bool HasFocus() const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::FocusTraversable* GetPaneFocusTraversable() override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

 private:
  void RemoveUserButtonPressed();

  LoginButton* bubble_opener_ = nullptr;
  base::RepeatingClosure on_remove_user_warning_shown_;
  base::RepeatingClosure on_remove_user_requested_;
  views::View* managed_user_data_ = nullptr;
  views::View* remove_user_confirm_data_ = nullptr;
  RemoveUserButton* remove_user_button_ = nullptr;
  views::Label* username_label_ = nullptr;
  views::Label* email_label_ = nullptr;
  views::Label* management_disclosure_label_ = nullptr;

  std::u16string warning_message_;

  std::unique_ptr<views::FocusSearch> focus_search_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_REMOVE_ACCOUNT_DIALOG_H_
