// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
#define ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_

#include <string>

#include "ash/login/ui/login_palette.h"
#include "ash/public/cpp/login_types.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
}  // namespace views

namespace ash {

class AnimatedRoundedImageView;
class LoginPasswordView;
class LoginPinView;
class LoginPinInputView;

// Contains the debug views that allows the developer to interact with the
// AuthDialogController.
class AuthDialogContentsView : public views::View {
 public:
  // Flags which describe the set of currently visible auth methods.
  enum AuthMethods {
    kAuthNone = 0,              // No auth methods.
    kAuthPassword = 1 << 0,     // Display password.
    kAuthPin = 1 << 1,          // Display PIN keyboard.
    kAuthFingerprint = 1 << 2,  // Use fingerprint to unlock.
  };

  // Extra control parameters to be passed when setting the auth methods.
  struct AuthMethodsMetadata {
    // User's pin length to use for autosubmit.
    size_t autosubmit_pin_length = 0;
  };

  AuthDialogContentsView(uint32_t auth_methods,
                         const AuthMethodsMetadata& auth_metadata,
                         const UserAvatar& avatar);
  AuthDialogContentsView(const AuthDialogContentsView&) = delete;
  AuthDialogContentsView& operator=(const AuthDialogContentsView&) = delete;
  ~AuthDialogContentsView() override;

  // views::Views:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  uint32_t auth_methods() const { return auth_methods_; }

 private:
  class FingerprintView;

  // views::View:
  void AddedToWidget() override;

  // Add a view for user avatar.
  void AddAvatarView(const UserAvatar& avatar);

  // Add a view for dialog title.
  void AddTitleView();

  // Add a view for the prompt message.
  void AddPromptView();

  // Add a view for entering PIN (if autosubmit is off).
  void AddPinTextInputView();

  // Add a PIN pad view.
  void AddPinPadView();

  // Add a PIN input view that automatically submits PIN.
  void AddPinDigitInputView();

  // Initializes password input field functionality.
  void InitPasswordView();

  // Add a vertical spacing view.
  void AddVerticalSpacing(int height);

  // Add a view for action buttons.
  void AddActionButtonsView();

  // Called when the user submits password or PIN.
  void OnAuthSubmit(const base::string16& password);

  // Called when PIN authentication of the user completes.
  void OnPinAuthComplete(base::Optional<bool> success);

  // Called when fingerprint authentication completes.
  void OnFingerprintAuthComplete(bool success,
                                 FingerprintState fingerprint_state);

  // Debug container which holds the entire debug UI.
  views::View* container_ = nullptr;

  // Layout for |container_|.
  views::BoxLayout* main_layout_ = nullptr;

  // User avatar to indicate this is an OS dialog.
  AnimatedRoundedImageView* avatar_view_ = nullptr;

  // Title of the auth dialog.
  views::Label* title_ = nullptr;

  // Prompt message to the user.
  views::Label* prompt_ = nullptr;

  // Whether PIN can be auto submitted.
  bool pin_autosubmit_on_ = false;

  // Text input field for PIN if PIN cannot be auto submitted.
  LoginPasswordView* pin_text_input_view_ = nullptr;

  // PIN input view that's shown if PIN can be auto submitted.
  LoginPinInputView* pin_digit_input_view_ = nullptr;

  // PIN pad view.
  LoginPinView* pin_pad_view_ = nullptr;

  FingerprintView* fingerprint_view_ = nullptr;

  // Flags of auth methods that should be visible.
  uint32_t auth_methods_ = 0u;

  // Extra parameters to control the UI.
  AuthMethodsMetadata auth_metadata_;

  LoginPalette palette_ = CreateInSessionAuthPalette();

  // Container which holds action buttons.
  views::View* action_view_container_ = nullptr;

  base::WeakPtrFactory<AuthDialogContentsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
