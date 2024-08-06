// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
#define ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
class LabelButton;
class MdTextButton;
}  // namespace views

namespace ash {

class AnimatedRoundedImageView;
class LoginPasswordView;
class LoginPinView;
class LoginPinInputView;

// Contains the debug views that allows the developer to interact with the
// AuthDialogController.
class AuthDialogContentsView : public views::View {
  METADATA_HEADER(AuthDialogContentsView, views::View)

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

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(AuthDialogContentsView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void PasswordOrPinAuthComplete(bool authenticated_by_pin,
                                   bool success,
                                   bool can_use_pin) const;

    void FingerprintAuthComplete(bool success,
                                 FingerprintState fingerprint_state) const;

    raw_ptr<LoginPasswordView> GetPasswordView() const;

    raw_ptr<LoginPasswordView> GetPinTextInputView() const;

    views::Label* GetDialogFingerprintLabel() const;

   private:
    const raw_ptr<AuthDialogContentsView> view_;
  };

  AuthDialogContentsView(uint32_t auth_methods,
                         const std::string& origin_name,
                         const AuthMethodsMetadata& auth_metadata,
                         const UserAvatar& avatar);
  AuthDialogContentsView(const AuthDialogContentsView&) = delete;
  AuthDialogContentsView& operator=(const AuthDialogContentsView&) = delete;
  ~AuthDialogContentsView() override;

  // views::Views:
  void RequestFocus() override;

  uint32_t auth_methods() const { return auth_methods_; }

 private:
  class TitleLabel;
  class FingerprintView;

  // views::View:
  void AddedToWidget() override;

  // Add a view for user avatar.
  void AddAvatarView(const UserAvatar& avatar);

  // Add a view for dialog title.
  void AddTitleView();

  // Add a view that shows which website/app we are authenticating for.
  void AddOriginNameView();

  // Add a view for entering PIN (if autosubmit is off).
  void AddPinTextInputView();

  // Add a view for entering password.
  void AddPasswordView();

  // Add a PIN pad view.
  void AddPinPadView();

  // Add a PIN input view that automatically submits PIN.
  void AddPinDigitInputView();

  // Add a vertical spacing view.
  void AddVerticalSpacing(int height);

  // Add a view for action buttons.
  void AddActionButtonsView();

  // Called when the user taps a digit on the PIN pad.
  void OnInsertDigitFromPinPad(int digit);

  // Called when the user taps backspace on the PIN pad.
  void OnBackspaceFromPinPad();

  // Called when either:
  // 1. the user inserts or deletes a character in
  // |pin_text_input_view_| or |pin_digit_input_view_| without using the PIN
  // pad, or
  // 2. the user inserts or deletes a character in |password_view_|, or
  // 3. contents of |pin_text_input_view_|, |password_view_|, or
  // |pin_digit_input_view_| are cleared by a Reset() call.
  void OnInputTextChanged(bool is_empty);

  // Called when the user submits password or PIN. If authenticated_by_pin is
  // false, the user authenticated by password.
  void OnAuthSubmit(bool authenticated_by_pin, const std::u16string& password);

  // Called when password or PIN authentication of the user completes. If
  // authenticated_by_pin is false, the user authenticated by password.
  // |can_use_pin| specifies whether PIN is available after the authentication,
  // as it might be locked out.
  void OnPasswordOrPinAuthComplete(bool authenticated_by_pin,
                                   bool success,
                                   bool can_use_pin);

  // Called when fingerprint authentication completes.
  void OnFingerprintAuthComplete(bool success,
                                 FingerprintState fingerprint_state);

  // Called when the cancel button is pressed.
  void OnCancelButtonPressed(const ui::Event& event);

  // Called when the "Need help?" button is pressed.
  void OnNeedHelpButtonPressed(const ui::Event& event);

  views::Label* GetFingerprintLabel() const;

  // Debug container which holds the entire debug UI.
  raw_ptr<views::View> container_ = nullptr;

  // Layout for |container_|.
  raw_ptr<views::BoxLayout> main_layout_ = nullptr;

  // User avatar to indicate this is an OS dialog.
  raw_ptr<AnimatedRoundedImageView> avatar_view_ = nullptr;

  // Title of the auth dialog, also used to show PIN auth error message..
  raw_ptr<TitleLabel> title_ = nullptr;

  // Prompt message to the user.
  raw_ptr<views::Label> origin_name_view_ = nullptr;

  // Whether PIN can be auto submitted.
  bool pin_autosubmit_on_ = false;

  // Whether PIN is locked out.
  bool pin_locked_out_ = false;

  // Text input field for PIN if PIN cannot be auto submitted.
  raw_ptr<LoginPasswordView> pin_text_input_view_ = nullptr;

  // PIN input view that's shown if PIN can be auto submitted.
  raw_ptr<LoginPinInputView> pin_digit_input_view_ = nullptr;

  // Text input field for password.
  raw_ptr<LoginPasswordView> password_view_ = nullptr;

  // PIN pad view.
  raw_ptr<LoginPinView> pin_pad_view_ = nullptr;

  raw_ptr<FingerprintView> fingerprint_view_ = nullptr;

  // A button to cancel authentication and close the dialog.
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;

  // A button to show a help center article.
  raw_ptr<views::LabelButton> help_button_ = nullptr;

  // Flags of auth methods that should be visible.
  uint32_t auth_methods_ = 0u;

  const std::string origin_name_;

  // Extra parameters to control the UI.
  AuthMethodsMetadata auth_metadata_;

  // Container which holds action buttons.
  raw_ptr<views::View> action_view_container_ = nullptr;

  base::WeakPtrFactory<AuthDialogContentsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTH_DIALOG_CONTENTS_VIEW_H_
