// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
#define ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_

#include <string>

#include "ash/public/cpp/login_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class LoginPasswordView;
class LoginPinView;

// Contains the debug views that allows the developer to interact with the
// AuthDialogController.
class AuthDialogContentsView : public views::View,
                               public views::ButtonListener {
 public:
  // Flags which describe the set of currently visible auth methods.
  enum AuthMethods {
    kAuthNone = 0,              // No auth methods.
    kAuthPassword = 1 << 0,     // Display password.
    kAuthPin = 1 << 1,          // Display PIN keyboard.
    kAuthFingerprint = 1 << 2,  // Use fingerprint to unlock.
  };

  explicit AuthDialogContentsView(uint32_t auth_methods);
  AuthDialogContentsView(const AuthDialogContentsView&) = delete;
  AuthDialogContentsView& operator=(const AuthDialogContentsView&) = delete;
  ~AuthDialogContentsView() override;

  // views::Views:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  uint32_t auth_methods() const { return auth_methods_; }

 private:
  class FingerprintView;

  // views::View:
  void AddedToWidget() override;

  // Add a view for dialog title.
  void AddTitleView();

  // Add a view for the prompt message.
  void AddPromptView();

  // Add a view for password input field.
  void AddPasswordView();

  // Add a PIN pad view.
  void AddPinView();

  // Initializes password input field functionality.
  void InitPasswordView();

  // Add a vertical spacing view.
  void AddVerticalSpacing(int height);

  // Add a view for action buttons.
  void AddActionButtonsView();

  // Creates a button on the debug row that cannot be focused.
  views::LabelButton* AddButton(const std::string& text,
                                int id,
                                views::View* container);

  // Called when the user submits password or PIN.
  void OnAuthSubmit(const base::string16& password);

  // Called when password/PIN authentication of the user completes.
  void OnPasswordOrPinAuthComplete(base::Optional<bool> success);

  // Called when fingerprint authentication completes.
  void OnFingerprintAuthComplete(bool success,
                                 FingerprintState fingerprint_state);

  // Debug container which holds the entire debug UI.
  views::View* container_ = nullptr;

  // Layout for |container_|.
  views::BoxLayout* main_layout_ = nullptr;

  // Title of the auth dialog.
  views::Label* title_ = nullptr;

  // Prompt message to the user.
  views::Label* prompt_ = nullptr;

  // Password input field for password and PIN.
  LoginPasswordView* password_view_ = nullptr;

  // PIN pad view.
  LoginPinView* pin_view_ = nullptr;

  FingerprintView* fingerprint_view_ = nullptr;

  // Flags of auth methods that should be visible.
  uint32_t auth_methods_ = 0u;

  // Cancel all operations and close th dialog.
  views::LabelButton* cancel_button_ = nullptr;

  // Container which holds action buttons.
  views::View* action_view_container_ = nullptr;

  base::WeakPtrFactory<AuthDialogContentsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
