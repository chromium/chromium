// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/login_arrow_navigation_delegate.h"
#include "ash/login/ui/login_error_bubble.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_status_message_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/style/pill_button.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

class DisabledAuthMessageView;
class LockedTpmMessageView;
class LoginAuthFactorsView;
class FingerprintAuthFactorModel;
class SmartLockAuthFactorModel;
class LoginPasswordView;
class LoginPinView;
class LoginPinInputView;
enum class SmartLockState;

// Wraps a UserView which also has authentication available. Adds additional
// views below the UserView instance which show authentication UIs.
//
// This class will make call mojo authentication APIs directly. The embedder can
// receive some events about the results of those mojo
// authentication attempts (ie, success/failure).
class ASH_EXPORT LoginAuthUserView : public NonAccessibleView {
  METADATA_HEADER(LoginAuthUserView, NonAccessibleView)

 public:
  // Flags which describe the set of currently visible auth methods.
  enum AuthMethods {
    AUTH_NONE = 0,                     // No extra auth methods.
    AUTH_PASSWORD = 1 << 0,            // Display password.
    AUTH_PIN = 1 << 1,                 // Display PIN keyboard.
    AUTH_ONLINE_SIGN_IN = 1 << 2,      // Force online sign-in.
    AUTH_FINGERPRINT = 1 << 3,         // Use fingerprint to unlock.
    AUTH_SMART_LOCK = 1 << 4,          // Use Smart Lock to unlock.
    AUTH_CHALLENGE_RESPONSE = 1 << 5,  // Authenticate via challenge-response
                                       // protocol using security token.
    AUTH_DISABLED = 1 << 6,  // Disable all the auth methods and show a
                             // message to user.
    AUTH_DISABLED_TPM_LOCKED = 1 << 7,  // Disable all the auth methods due
                                        // to the TPM being locked
    AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD =
        1 << 8,  // Hide the password/pin fields and slide the auth factors
                 // up. This happens, for example,  when an auth factor requires
                 // the user to click a button as a final step. Note that if
                 // this bit is set, the password/pin will be hidden even if
                 // AUTH_PASSWORD and/or AUTH_PIN are set.
    AUTH_RECOVERY = 1 << 9,  // Shows the recovery user button when PIN is
                             // locked and it is the only auth factor.
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
    std::optional<base::TimeDelta> time_until_tpm_unlock = std::nullopt;
    // Only present when the PIN is soft locked. If not present, it means the
    // PIN is enabled or disabled permanently.
    cryptohome::PinLockAvailability pin_available_at = std::nullopt;
  };

  // Possible states that the input fields (PasswordView & PinInputView)
  // might be in. This is determined by the current authentication methods
  // that a user has.
  enum class InputFieldMode {
    kNone,                        // Not showing any input field.
    kPasswordOnly,                // Password only field, no PIN set.
    kPinOnlyAutosubmitOn,         // PIN only field, auto-submit feature
                                  // enabled, no password set.
    kPinOnlyAutosubmitOff,        // PIN only field, auto-submit feature
                                  // disabled, no password set.
    kPasswordWithToggle,          // Password field with toggle to switch to PIN
                                  // field.
    kPinWithToggleAutosubmitOn,   // PIN field with toggle, auto-submit
                                  // feature enabled.
    kPinWithToggleAutosubmitOff,  // PIN field with toggle, auto-submit feature
                                  // disabled.
    // TODO(b/357606198): Separate password and PIN field.
    kPasswordAndPin,  // Both password and PIN are set, PIN auto-submit feature
                      // disabled.
  };

  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginAuthUserView* view);
    ~TestApi();

    LoginUserView* user_view() const;
    LoginRemoveAccountDialog* remove_account_dialog() const;
    LoginPasswordView* password_view() const;
    LoginPinView* pin_view() const;
    LoginPinInputView* pin_input_view() const;
    views::Button* pin_password_toggle() const;
    views::LabelButton* online_sign_in_message() const;
    views::View* disabled_auth_message() const;
    views::Button* challenge_response_button();
    views::Label* challenge_response_label();
    LoginAuthFactorsView* auth_factors_view() const;
    AuthFactorModel* fingerprint_auth_factor_model() const;
    AuthFactorModel* smart_lock_auth_factor_model() const;
    PinStatusMessageView* pin_status_message_view() const;
    bool HasAuthMethod(AuthMethods auth_method) const;
    const std::u16string& GetDisabledAuthMessageContent() const;
    void SetFingerprintState(FingerprintState state) const;
    void SetSmartLockState(SmartLockState state) const;
    void ShowDialog();
    const std::u16string& GetPinStatusMessageContent() const;

   private:
    const raw_ptr<LoginAuthUserView, DanglingUntriaged> view_;
  };

  using OnAuthCallback =
      base::RepeatingCallback<void(bool auth_success,
                                   bool display_error_messages,
                                   bool authenticated_by_pin)>;

  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Executed whenever an authentication result is available, such as when the
    // user submits a password or clicks to complete Smart Lock.
    OnAuthCallback on_auth;
    // Called when the user taps the user view.
    LoginUserView::OnTap on_tap;
    // Called when the remove user warning message has been shown.
    LoginUserView::OnRemoveWarningShown on_remove_warning_shown;
    // Called when the user should be removed. The callback should do the actual
    // removal.
    LoginUserView::OnRemove on_remove;
    // Called when LoginAuthFactorsView enters/exits a state where an auth
    // factor wants to hide the password and pin.
    base::RepeatingCallback<void(bool)>
        on_auth_factor_is_hiding_password_changed;
    // Called when the pin becomes available after being soft-locked due to
    // multiple wrong pin attempts.
    PinStatusMessageView::OnPinUnlock on_pin_unlock;
    // Called when the recover user button is pressed.
    base::RepeatingClosure on_recover_button_pressed;
  };

  LoginAuthUserView(const LoginUserInfo& user, const Callbacks& callbacks);

  LoginAuthUserView(const LoginAuthUserView&) = delete;
  LoginAuthUserView& operator=(const LoginAuthUserView&) = delete;

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

  // Reset the fingerprint state by updating UI to reflect the current state.
  void ResetFingerprintUIState();

  // Called to show a fingerprint authentication attempt result.
  void NotifyFingerprintAuthResult(bool success);

  // Update the current Smart Lock state.
  void SetSmartLockState(SmartLockState state);

  // Called to show a Smart Lock authentication attempt result.
  void NotifySmartLockAuthResult(bool success);

  // Set the parameters needed to render the message that is shown to user when
  // auth method is |AUTH_DISABLED|.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data);

  const LoginUserInfo& current_user() const;

  // Provides the view that should be the anchor to message bubbles. Either the
  // password field, or the PIN field.
  base::WeakPtr<views::View> GetActiveInputView();
  LoginPasswordView* password_view() { return password_view_; }
  LoginUserView* user_view() { return user_view_; }
  views::Button* pin_password_toggle() { return pin_password_toggle_; }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void RequestFocus() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class LoginAuthUserViewTestBase;

  struct UiState;
  class ChallengeResponseView;

  // Called when the user submits an auth method. Runs mojo call.
  void OnAuthSubmit(const std::u16string& password);
  // Called with the result of the request started in |OnAuthSubmit| or
  // |AttemptAuthenticateWithExternalBinary|.
  void OnAuthComplete(bool authenticated_by_pin,
                      std::optional<bool> auth_success);
  // Called with the result of the request started in
  // |AttemptAuthenticateWithChallengeResponse|.
  void OnChallengeResponseAuthComplete(std::optional<bool> auth_success);

  // Create a new LoginRemoveAccountDialog for the user and make ot visible.
  // Existing dialog has to be deleted before this call.
  void ShowRemoveAccountDialog();

  // Delete the existing LoginRemoveAccountDialog, if present.
  void DeleteRemoveAccountDialog();

  // Dropdown icon was pressed in the user view.
  void OnAccountRemovalRequested();

  // Called when the LoginAuthFactorsView "arrow button" is tapped for the
  // Smart Lock auth factor; the user's phone is unlocked and the user has
  // tapped this button in order to authenticate with Smart Lock.
  void OnSmartLockArrowButtonTapped();

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

  // Called when the recover button is pressed.
  void OnRecoverButtonPressed();

  // Helper method to check if an auth method is enable. Use it like this:
  // bool has_tap = HasAuthMethod(AUTH_PASSWORD).
  bool HasAuthMethod(AuthMethods auth_method) const;

  // Whether the authentication attempt should use the user's password only.
  bool ShouldAuthenticateWithPassword() const;

  // Whether the authentication attempt should use the user's PIN.
  bool ShouldAuthenticateWithPin() const;

  // TODO(crbug.com/41423180): remove this and pass a handler in via the
  // Callbacks struct instead.
  void AttemptAuthenticateWithExternalBinary();

  // Called when the user triggered the challenge-response authentication. It
  // starts the asynchronous authentication process against a security token.
  void AttemptAuthenticateWithChallengeResponse();

  // Requests focus on the password view and shows the virtual keyboard if
  // enabled and if the PIN pad is not shown already.
  void RequestFocusOnPasswordView();

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
  bool ShouldShowPinStatusMessage() const;

  // Convenience methods to determine the necessary paddings.
  gfx::Size GetPaddingBelowUserView() const;
  gfx::Size GetPaddingBelowPasswordView() const;

  // Convenience methods to determine UI text based on the InputFieldMode.
  std::u16string GetPinPasswordToggleText() const;
  std::u16string GetPasswordViewPlaceholder() const;
  std::u16string GetMultiUserSignInDisableAuthMessage() const;

  // Authentication methods available and extra parameters that control the UI.
  AuthMethods auth_methods_ = AUTH_NONE;
  AuthMethodsMetadata auth_metadata_ = AuthMethodsMetadata();

  // Controls which input field is currently being shown.
  InputFieldMode input_field_mode_ = InputFieldMode::kNone;

  raw_ptr<LoginUserView> user_view_ = nullptr;
  raw_ptr<LoginPasswordView> password_view_ = nullptr;
  raw_ptr<LoginPinInputView> pin_input_view_ = nullptr;
  raw_ptr<PillButton> pin_password_toggle_ = nullptr;
  raw_ptr<LoginPinView> pin_view_ = nullptr;
  raw_ptr<views::LabelButton> online_sign_in_button_ = nullptr;
  raw_ptr<DisabledAuthMessageView> disabled_auth_message_ = nullptr;
  raw_ptr<LoginAuthFactorsView> auth_factors_view_ = nullptr;
  raw_ptr<FingerprintAuthFactorModel> fingerprint_auth_factor_model_ = nullptr;
  raw_ptr<SmartLockAuthFactorModel> smart_lock_auth_factor_model_ = nullptr;
  raw_ptr<ChallengeResponseView> challenge_response_view_ = nullptr;
  raw_ptr<LockedTpmMessageView> locked_tpm_message_view_ = nullptr;
  raw_ptr<PinStatusMessageView> pin_status_message_view_ = nullptr;
  raw_ptr<views::LabelButton> recover_button_ = nullptr;

  // Padding below the user view. Grows when there isn't an input field
  // or smart card login.
  raw_ptr<NonAccessibleView> padding_below_user_view_ = nullptr;
  // Displays padding between:
  // 1. Password field and pin keyboard
  // 2. Password field and fingerprint view, when pin is not available.
  // Preferred size will change base on current auth method.
  raw_ptr<NonAccessibleView> padding_below_password_view_ = nullptr;

  // Bubble used for displaying the user remove account dialog. Its parent is
  // the top level view, either LockContentsView or LockDebugView. This allows
  // the remove account dialog to be clicked outside the bounds of the user
  // view.
  std::unique_ptr<LoginRemoveAccountDialog> remove_account_dialog_;

  const OnAuthCallback on_auth_;
  const LoginUserView::OnTap on_tap_;
  const LoginUserView::OnRemoveWarningShown on_remove_warning_shown_;
  const LoginUserView::OnRemove on_remove_;
  const PinStatusMessageView::OnPinUnlock on_pin_unlock_;
  const base::RepeatingClosure on_recover_button_pressed_;

  // UI state that was stored before setting new authentication methods.
  // Generated by `CaptureStateForAnimationPreLayout` and consumed by
  // `ApplyAnimationPostLayout`.
  std::unique_ptr<UiState> previous_state_;

  // The delegate of the password field's arrow keys.
  std::unique_ptr<LoginScreenArrowNavigationDelegate>
      arrow_navigation_delegate_;

  base::WeakPtrFactory<LoginAuthUserView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
