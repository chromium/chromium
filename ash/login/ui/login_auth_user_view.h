// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_

#include <stdint.h>
#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

class LoginPasswordView;
class LoginPinView;
class LoginPinInputView;

// Wraps a UserView which also has authentication available. Adds additional
// views below the UserView instance which show authentication UIs.
//
// This class will make call mojo authentication APIs directly. The embedder can
// receive some events about the results of those mojo
// authentication attempts (ie, success/failure).
class ASH_EXPORT LoginAuthUserView : public NonAccessibleView,
                                     public views::ButtonListener {
 public:
  // Flags which describe the set of currently visible auth methods.
  enum AuthMethods {
    AUTH_NONE = 0,                     // No extra auth methods.
    AUTH_PASSWORD = 1 << 0,            // Display password.
    AUTH_PIN = 1 << 1,                 // Display PIN keyboard.
    AUTH_TAP = 1 << 2,                 // Tap to unlock.
    AUTH_ONLINE_SIGN_IN = 1 << 3,      // Force online sign-in.
    AUTH_FINGERPRINT = 1 << 4,         // Use fingerprint to unlock.
    AUTH_CHALLENGE_RESPONSE = 1 << 5,  // Authenticate via challenge-response
                                       // protocol using security token.
    AUTH_DISABLED = 1 << 6,  // Disable all the auth methods and show a
                             // message to user.
    AUTH_DISABLED_TPM_LOCKED = 1 << 7,  // Disable all the auth methods due
                                        // to the TPM being locked
  };

  // Extra control parameters to be passed when setting the auth methods.
  struct AuthMethodsMetadata {
    AuthMethodsMetadata();
    ~AuthMethodsMetadata();
    AuthMethodsMetadata(const AuthMethodsMetadata&);

    // If the virtual keyboard is visible, the pinpad is hidden.
    bool virtual_keyboard_visible = false;
    // Whether to show the pinpad for the password field.
    bool show_pinpad_for_pw = false;
    // User's pin length to use for autosubmit.
    size_t autosubmit_pin_length = 0;
    // Only present when the TPM is locked.
    base::Optional<base::TimeDelta> time_until_tpm_unlock = base::nullopt;
  };

  // Possible states that the input fields (PasswordView & PinInputView)
  // might be in. This is determined by the current authentication methods
  // that a user has.
  enum class InputFieldMode {
    NONE,              // Not showing any input field.
    PASSWORD_ONLY,     // No PIN set. Password only field.
    PIN_AND_PASSWORD,  // PIN set, but auto-submit feature disabled.
    PIN_WITH_TOGGLE,   // PIN field for auto submit.
    PWD_WITH_TOGGLE    // PWD field when auto submit enabled.
  };

  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginAuthUserView* view);
    ~TestApi();

    LoginUserView* user_view() const;
    LoginPasswordView* password_view() const;
    LoginPinView* pin_view() const;
    LoginPinInputView* pin_input_view() const;
    views::Button* pin_password_toggle() const;
    views::Button* online_sign_in_message() const;
    views::View* disabled_auth_message() const;
    views::Button* challenge_response_button();
    views::Label* challenge_response_label();
    bool HasAuthMethod(AuthMethods auth_method) const;
    const base::string16& GetDisabledAuthMessageContent() const;

   private:
    LoginAuthUserView* const view_;
  };

  using OnAuthCallback =
      base::RepeatingCallback<void(bool auth_success,
                                   bool display_error_messages)>;
  using OnEasyUnlockIconTapped = base::RepeatingClosure;
  using OnEasyUnlockIconHovered = base::RepeatingClosure;

  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Executed whenever an authentication result is available, such as when the
    // user submits a password or taps the user icon when AUTH_TAP is enabled.
    OnAuthCallback on_auth;
    // Called when the user taps the user view and AUTH_TAP is not enabled.
    LoginUserView::OnTap on_tap;
    // Called when the remove user warning message has been shown.
    LoginUserView::OnRemoveWarningShown on_remove_warning_shown;
    // Called when the user should be removed. The callback should do the actual
    // removal.
    LoginUserView::OnRemove on_remove;
    // Called when the easy unlock icon is hovered.
    OnEasyUnlockIconHovered on_easy_unlock_icon_hovered;
    // Called when the easy unlock icon is tapped.
    OnEasyUnlockIconTapped on_easy_unlock_icon_tapped;
  };

  LoginAuthUserView(const LoginUserInfo& user, const Callbacks& callbacks);
  ~LoginAuthUserView() override;

  // Set the displayed set of auth methods. |auth_methods| contains or-ed
  // together AuthMethod values. |auth_metadata| provides additional control
  // parameters for the view. Must always be called in conjunction with
  // `CaptureStateForAnimationPreLayout` and `ApplyAnimationPostLayout`.
  void SetAuthMethods(
      uint32_t auth_methods,
      const AuthMethodsMetadata& auth_metadata = AuthMethodsMetadata());
  AuthMethods auth_methods() const { return auth_methods_; }
  InputFieldMode input_field_mode() const { return input_field_mode_; }

  // Add an easy unlock icon.
  void SetEasyUnlockIcon(EasyUnlockIconId id,
                         const base::string16& accessibility_label);

  // Captures any metadata about the current view state that will be used for
  // animation.
  void CaptureStateForAnimationPreLayout();
  // Applies animation based on current layout state compared to the most
  // recently captured state. If `animate` is false, the previous UI state
  // is released and no animation is performed.
  void ApplyAnimationPostLayout(bool animate);

  // Update the displayed name, icon, etc to that of |user|.
  void UpdateForUser(const LoginUserInfo& user);

  // Update the current fingerprint state.
  void SetFingerprintState(FingerprintState state);

  // Called to show a fingerprint authentication attempt result.
  void NotifyFingerprintAuthResult(bool success);

  // Set the parameters needed to render the message that is shown to user when
  // auth method is |AUTH_DISABLED|.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data);

  const LoginUserInfo& current_user() const;

  // Provides the view that should be the anchor to message bubbles. Either the
  // password field, or the PIN field.
  views::View* GetActiveInputView();
  LoginPasswordView* password_view() { return password_view_; }
  LoginUserView* user_view() { return user_view_; }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  struct UiState;
  class FingerprintView;
  class ChallengeResponseView;
  class DisabledAuthMessageView;
  class LockedTpmMessageView;

  // Called when the user submits an auth method. Runs mojo call.
  void OnAuthSubmit(const base::string16& password);
  // Called with the result of the request started in |OnAuthSubmit| or
  // |AttemptAuthenticateWithExternalBinary|.
  void OnAuthComplete(base::Optional<bool> auth_success);
  // Called with the result of the request started in
  // |AttemptAuthenticateWithChallengeResponse|.
  void OnChallengeResponseAuthComplete(base::Optional<bool> auth_success);

  // Called when the user view has been tapped. This will run |on_auth_| if tap
  // to unlock is enabled, or run |OnOnlineSignInMessageTap| if the online
  // sign-in message is shown, otherwise it will run |on_tap_|.
  void OnUserViewTap();

  // Called when the online sign-in message is tapped. It opens the Gaia screen.
  void OnOnlineSignInMessageTap();

  // Called from LoginPinView, forwards the calls to the active input field.
  void OnPinPadBackspace();
  void OnPinPadInsertDigit(int digit);
  // Called from both input fields, forwards the call to LoginPinView (pin pad)
  void OnPasswordTextChanged(bool is_empty);
  void OnPinTextChanged(bool is_empty);

  // Helper method to check if an auth method is enable. Use it like this:
  // bool has_tap = HasAuthMethod(AUTH_TAP).
  bool HasAuthMethod(AuthMethods auth_method) const;

  // TODO(crbug/899812): remove this and pass a handler in via the Callbacks
  // struct instead.
  void AttemptAuthenticateWithExternalBinary();

  // Called when the user triggered the challenge-response authentication. It
  // starts the asynchronous authentication process against a security token.
  void AttemptAuthenticateWithChallengeResponse();

  // Updates the element in focus. Used in `ApplyAnimationPostLayout`.
  void UpdateFocus();

  // Updates the UI internally when the switch button is clicked to toggle
  // between pin and password.
  void OnSwitchButtonClicked();

  // Determines the mode of the input field based on the available
  // authentication methods.
  void UpdateInputFieldMode();

  // Convenience methods to determine element visibility.
  bool ShouldShowPinPad() const;
  bool ShouldShowPasswordField() const;
  bool ShouldShowPinInputField() const;
  bool ShouldShowToggle() const;

  // Convenience methods to determine the necessary paddings.
  gfx::Size GetPaddingBelowUserView() const;
  gfx::Size GetPaddingBelowPasswordView() const;

  // Convenience methods to determine UI text based on the InputFieldMode.
  base::string16 GetPinPasswordToggleText();
  base::string16 GetPasswordViewPlaceholder() const;

  // Authentication methods available and extra parameters that control the UI.
  AuthMethods auth_methods_ = AUTH_NONE;
  AuthMethodsMetadata auth_metadata_ = AuthMethodsMetadata();

  // Controls which input field is currently being shown.
  InputFieldMode input_field_mode_ = InputFieldMode::NONE;

  LoginUserView* user_view_ = nullptr;
  LoginPasswordView* password_view_ = nullptr;
  NonAccessibleView* password_view_container_ = nullptr;
  LoginPinInputView* pin_input_view_ = nullptr;
  views::LabelButton* pin_password_toggle_ = nullptr;
  LoginPinView* pin_view_ = nullptr;
  views::LabelButton* online_sign_in_message_ = nullptr;
  DisabledAuthMessageView* disabled_auth_message_ = nullptr;
  FingerprintView* fingerprint_view_ = nullptr;
  ChallengeResponseView* challenge_response_view_ = nullptr;
  LockedTpmMessageView* locked_tpm_message_view_ = nullptr;

  // Padding below the user view. Grows when there isn't an input field
  // or smart card login.
  NonAccessibleView* padding_below_user_view_ = nullptr;
  // Displays padding between:
  // 1. Password field and pin keyboard
  // 2. Password field and fingerprint view, when pin is not available.
  // Preferred size will change base on current auth method.
  NonAccessibleView* padding_below_password_view_ = nullptr;
  const OnAuthCallback on_auth_;
  const LoginUserView::OnTap on_tap_;

  // UI state that was stored before setting new authentication methods.
  // Generated by `CaptureStateForAnimationPreLayout` and consumed by
  // `ApplyAnimationPostLayout`.
  std::unique_ptr<UiState> previous_state_;

  base::WeakPtrFactory<LoginAuthUserView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginAuthUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
