// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Textfield;
class ToggleImageButton;
}  // namespace views

namespace ash {
class ArrowButtonView;
class LoginArrowNavigationDelegate;

// Contains a textfield and a submit button. When the display password button
// is visible, the textfield contains a button in the form of an eye icon that
// the user can click on to reveal the password. Submitting a password will
// make it read only and prevent further submissions until the controller sets
// ReadOnly to false again.
//
// This view is always rendered via layers.
//
//
// When the display password button is hidden, the password view looks
// like this:
//
// * * * * * *         (=>)
// ------------------
//
// When the display password button is visible, the password view looks
// like this by default:
//
//  * * * * * *    (\)  (=>)
//  ------------------
//
//  or this, in display mode:
//
//  1 2 3 4 5 6    (o)  (=>)
//  ------------------
class ASH_EXPORT LoginPasswordView : public views::View,
                                     public views::TextfieldController,
                                     public ImeController::Observer,
                                     public ui::ImplicitAnimationObserver {
  METADATA_HEADER(LoginPasswordView, views::View)

 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPasswordView* view);
    ~TestApi();

    void SubmitPassword(const std::string& password);

    views::Textfield* textfield() const;
    views::View* submit_button() const;
    views::ToggleImageButton* display_password_button() const;

   private:
    raw_ptr<LoginPasswordView> view_;
  };

  using OnPasswordSubmit =
      base::RepeatingCallback<void(const std::u16string& password)>;
  using OnPasswordTextChanged = base::RepeatingCallback<void(bool is_empty)>;

  // Must call |Init| after construction.
  LoginPasswordView();

  LoginPasswordView(const LoginPasswordView&) = delete;
  LoginPasswordView& operator=(const LoginPasswordView&) = delete;

  ~LoginPasswordView() override;

  // |on_submit| is called when the user hits enter or has pressed the submit
  // arrow.
  // |on_password_text_changed| is called when the text in the password field
  // changes.
  void Init(const OnPasswordSubmit& on_submit,
            const OnPasswordTextChanged& on_password_text_changed);

  // Enable or disable focus on the child elements (i.e.: password field and
  // submit button, or display password button if it is shown).
  void SetFocusEnabledForTextfield(bool enable);

  // Sets whether the display password button is visible.
  void SetDisplayPasswordButtonVisible(bool visible);

  // Clear the text and put the password into hide mode.
  void Reset();

  // Inserts the given numeric value to the textfield at the current cursor
  // position (most likely the end).
  void InsertNumber(int value);

  // Erase the last entered value.
  void Backspace();

  // Set password field placeholder. The password view cannot set the text by
  // itself because it doesn't know which auth methods are enabled.
  void SetPlaceholderText(const std::u16string& placeholder_text);

  // Makes the textfield read-only and enables/disables submitting.
  void SetReadOnly(bool read_only);
  bool IsReadOnly() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Invert the textfield type and toggle the display password button.
  void InvertPasswordDisplayingState();

  // Hides the password. When |chromevox_exception| is true, the password is not
  // hidden if ChromeVox is enabled. There should be a ChromeVox exception iff
  // it is triggered by a timer: a user action or a reset call should always
  // hide password.
  void HidePassword(bool chromevox_exception);

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  void HandleLeftIconsVisibilities(bool handling_capslock);

  // Submits the current password field text to mojo call and resets the text
  // field.
  void SubmitPassword();

  // Sets the delegate of the arrow keys navigation.
  void SetLoginArrowNavigationDelegate(LoginArrowNavigationDelegate* delegate);

  void SetAccessibleNameOnTextfield(const std::u16string& new_name);

  base::WeakPtr<LoginPasswordView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class DisplayPasswordButton;
  class LoginPasswordRow;
  class LoginTextfield;
  friend class TestApi;

  // Increases/decreases the contrast of the capslock icon.
  void SetCapsLockHighlighted(bool highlight);

  // Needs to be true in order for SubmitPassword to be ran. Returns true if the
  // textfield is not empty or if |enabled_on_empty_password| is true.
  bool IsPasswordSubmittable();

  // UpdateUiState enables/disables the submit button, and the display password
  // button when it is visible.
  void UpdateUiState();

  OnPasswordSubmit on_submit_;
  OnPasswordTextChanged on_password_text_changed_;

  // Arrow keystrokes delegate.
  raw_ptr<LoginArrowNavigationDelegate, DanglingUntriaged>
      arrow_navigation_delegate_ = nullptr;

  // Clears the password field after a time without action if the display
  // password button is visible.
  base::RetainingOneShotTimer clear_password_timer_;

  // Hides the password after a short delay if the password is shown, except if
  // ChromeVox is enabled (otherwise, the user would not have time to navigate
  // through the password and make the characters read out loud one by one).
  base::RetainingOneShotTimer hide_password_timer_;

  raw_ptr<LoginPasswordRow> password_row_ = nullptr;
  raw_ptr<LoginTextfield> textfield_ = nullptr;
  raw_ptr<ArrowButtonView> submit_button_ = nullptr;
  raw_ptr<DisplayPasswordButton> display_password_button_ = nullptr;
  raw_ptr<views::ImageView> capslock_icon_ = nullptr;

  base::WeakPtrFactory<LoginPasswordView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PASSWORD_VIEW_H_
