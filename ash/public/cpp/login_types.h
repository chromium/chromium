// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_TYPES_H_
#define ASH_PUBLIC_CPP_LOGIN_TYPES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "base/token.h"
#include "chromeos/components/proximity_auth/public/mojom/auth_type.mojom-forward.h"
#include "chromeos/constants/security_token_pin_types.h"
#include "components/account_id/account_id.h"

namespace ash {

// State of the Oobe UI dialog, which is used to update the visibility of login
// shelf buttons.
// This comes from SIGNIN_UI_STATE defined in display_manager.js, with an
// additional value HIDDEN to indicate the visibility of the oobe ui dialog.
enum class OobeDialogState {
  // Showing other screen, which does not impact the visibility of login shelf
  // buttons.
  NONE = 0,

  // Showing gaia signin screen.
  GAIA_SIGNIN = 1,

  // 2 is unused to keep in sync with display_manager.js

  // Showing wrong hardware identification screen.
  WRONG_HWID_WARNING = 3,

  // Showing supervised user creation screen.
  SUPERVISED_USER_CREATION_FLOW = 4,

  // Showing SAML password confirmation screen.
  SAML_PASSWORD_CONFIRM = 5,

  // Showing password changed screen.
  PASSWORD_CHANGED = 6,

  // Showing device enrollment screen.
  ENROLLMENT = 7,

  // Showing error screen.
  ERROR = 8,

  // Showing sync consent screen.
  SYNC_CONSENT = 9,

  // Oobe UI dialog is currently hidden.
  HIDDEN = 10,

  // Showing login UI provided by a Chrome extension using chrome.loginScreenUi
  // API.
  EXTENSION_LOGIN = 11,
};

// Supported multi-profile user behavior values.
// Keep in sync with the enum in chromeos_user_pod_row.js and user_pod_row.js
// TODO(estade): change all the enums to use kCamelCase.
enum class MultiProfileUserBehavior {
  UNRESTRICTED = 0,
  PRIMARY_ONLY = 1,
  NOT_ALLOWED = 2,
  OWNER_PRIMARY_ONLY = 3,
};

// Easy unlock icon choices.
enum class EasyUnlockIconId {
  // No icon shown.
  NONE,
  // The user has clicked the easy unlock icon and disabled easy unlock for this
  // login/lock session.
  HARDLOCKED,
  // Phone could not be found.
  LOCKED,
  // Phone found, but it is not unlocked.
  LOCKED_TO_BE_ACTIVATED,
  // Phone found, but it is too far away.
  LOCKED_WITH_PROXIMITY_HINT,
  // Phone found and unlocked. The user can click to dismiss the login/lock
  // screen.
  UNLOCKED,
  // Scanning for phone.
  SPINNER,
};

// The status of fingerprint availability.
enum class FingerprintState {
  // The user cannot use fingerprint. This may be because:
  //  - they are not the primary user
  //  - they never registered fingerprint
  //  - the device does not have a fingerprint sensor
  UNAVAILABLE,
  // Fingerprint can be used to unlock the device.
  AVAILABLE,
  // There have been too many attempts, so now fingerprint is disabled.
  DISABLED_FROM_ATTEMPTS,
  // It has been too long since the device was last used.
  DISABLED_FROM_TIMEOUT,
  kMaxValue = DISABLED_FROM_TIMEOUT,
};

// Information about the custom icon in the user pod.
struct ASH_PUBLIC_EXPORT EasyUnlockIconOptions {
  EasyUnlockIconOptions();
  EasyUnlockIconOptions(const EasyUnlockIconOptions& other);
  EasyUnlockIconOptions(EasyUnlockIconOptions&& other);
  ~EasyUnlockIconOptions();

  EasyUnlockIconOptions& operator=(const EasyUnlockIconOptions& other);
  EasyUnlockIconOptions& operator=(EasyUnlockIconOptions&& other);

  // Icon that should be displayed.
  EasyUnlockIconId icon = EasyUnlockIconId::NONE;
  // Tooltip that is associated with the icon. This is shown automatically if
  // |autoshow_tooltip| is true. The user can always see the tooltip if they
  // hover over the icon. The tooltip should be used for the accessibility label
  // if it is present.
  base::string16 tooltip;
  // If true, the tooltip should be displayed (even if the user is not currently
  // hovering over the icon, ie, this makes |tooltip| act like a little like a
  // notification).
  bool autoshow_tooltip = false;
  // Accessibility label. Only used if |tooltip| is empty.
  // TODO(jdufault): Always populate and use |aria_label|, even if |tooltip| is
  // non-empty.
  base::string16 aria_label;
  // If true, clicking the easy unlock icon should fire a hardlock event which
  // will disable easy unlock. The hardlock event will request a new icon
  // display via a separate EasyUnlockIconsOption update. See
  // LoginScreenClient::HardlockPod.
  bool hardlock_on_click = false;
};

// Information of each input method. This is used to populate keyboard layouts
// for public account user.
struct ASH_PUBLIC_EXPORT InputMethodItem {
  InputMethodItem();
  InputMethodItem(const InputMethodItem& other);
  InputMethodItem(InputMethodItem&& other);
  ~InputMethodItem();

  InputMethodItem& operator=(const InputMethodItem& other);
  InputMethodItem& operator=(InputMethodItem&& other);

  // An id that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string ime_id;

  // Title of the input method.
  std::string title;

  // Whether this input method is been selected.
  bool selected = false;
};

// Information of each available locale. This is used to populate language
// locales for public account user.
struct ASH_PUBLIC_EXPORT LocaleItem {
  LocaleItem();
  LocaleItem(const LocaleItem& other);
  LocaleItem(LocaleItem&& other);
  ~LocaleItem();

  LocaleItem& operator=(const LocaleItem& other);
  LocaleItem& operator=(LocaleItem&& other);

  // Language code of the locale.
  std::string language_code;

  // Title of the locale.
  std::string title;

  // Group name of the locale.
  base::Optional<std::string> group_name;
};

// Information about a public account user.
struct ASH_PUBLIC_EXPORT PublicAccountInfo {
  PublicAccountInfo();
  PublicAccountInfo(const PublicAccountInfo& other);
  PublicAccountInfo(PublicAccountInfo&& other);
  ~PublicAccountInfo();

  PublicAccountInfo& operator=(const PublicAccountInfo& other);
  PublicAccountInfo& operator=(PublicAccountInfo&& other);

  // The domain name displayed in the login screen UI.
  base::Optional<std::string> enterprise_domain;

  // A list of available user locales.
  std::vector<LocaleItem> available_locales;

  // Default locale for this user.
  std::string default_locale;

  // Show expanded user view that contains session information/warnings and
  // locale selection.
  bool show_expanded_view = false;

  // Show the advanced expanded user view if there are at least two recommended
  // locales. This will be the case in multilingual environments where users
  // are likely to want to choose among locales.
  bool show_advanced_view = false;

  // A list of available keyboard layouts.
  std::vector<InputMethodItem> keyboard_layouts;

  // Whether public account uses SAML authentication.
  bool using_saml = false;
};

// Info about a user in login/lock screen.
struct ASH_PUBLIC_EXPORT LoginUserInfo {
  LoginUserInfo();
  LoginUserInfo(const LoginUserInfo& other);
  LoginUserInfo(LoginUserInfo&& other);
  ~LoginUserInfo();

  LoginUserInfo& operator=(const LoginUserInfo& other);
  LoginUserInfo& operator=(LoginUserInfo&& other);

  // User's basic information including account id, email, avatar etc.
  UserInfo basic_user_info;

  // What method the user can use to sign in.
  // Initialized in .cc file because the mojom header is huge.
  proximity_auth::mojom::AuthType auth_type;

  // True if this user has already signed in.
  bool is_signed_in = false;

  // True if this user is the device owner.
  bool is_device_owner = false;

  // The initial fingerprint state. There are other methods (ie,
  // LoginScreenModel::SetFingerprintState) which update the current state.
  FingerprintState fingerprint_state = FingerprintState::UNAVAILABLE;

  // True if multi-profiles sign in is allowed for this user.
  bool is_multiprofile_allowed = false;

  // Enforced policy for multi-profiles sign in.
  MultiProfileUserBehavior multiprofile_policy =
      MultiProfileUserBehavior::UNRESTRICTED;

  // True if this user can be removed.
  bool can_remove = false;

  // Show pin pad for password for this user or not.
  bool show_pin_pad_for_password = false;

  // Contains the public account information if user type is PUBLIC_ACCOUNT.
  base::Optional<PublicAccountInfo> public_account_info;
};

enum class AuthDisabledReason {
  // Auth is disabled because the device is locked by a time limit override.
  kTimeLimitOverride,

  // Auth is disabled because the user has reached their daily usage limit on
  // the device.
  kTimeUsageLimit,

  // Auth is disabled because the device is within a locked time window.
  kTimeWindowLimit,
};

// The data needed to customize the lock screen when auth is disabled.
struct ASH_PUBLIC_EXPORT AuthDisabledData {
  AuthDisabledData();
  AuthDisabledData(AuthDisabledReason reason,
                   const base::Time& auth_reenabled_time,
                   const base::TimeDelta& device_used_time,
                   bool disable_lock_screen_media);
  AuthDisabledData(const AuthDisabledData& other);
  AuthDisabledData(AuthDisabledData&& other);
  ~AuthDisabledData();

  AuthDisabledData& operator=(const AuthDisabledData& other);
  AuthDisabledData& operator=(AuthDisabledData&& other);

  // Reason why auth is disabled.
  AuthDisabledReason reason = AuthDisabledReason::kTimeLimitOverride;

  // A future time when auth will be enabled. This value is for display purpose
  // only, auth won't be automatically enabled when this time is reached.
  base::Time auth_reenabled_time;

  // The amount of time that the user used this device.
  base::TimeDelta device_used_time;

  // If true media will be suspended and media controls will be unavailable on
  // lock screen.
  bool disable_lock_screen_media = false;
};

// Possible reasons why the parent access code is required. This corresponds to
// actions that children can't perform on a Chromebook, but their parents can on
// their behalf.
enum class ParentAccessRequestReason {
  // Unlock a Chromebook that is locked due to a Time Limit policy.
  kUnlockTimeLimits,
  // Update values on the date time dialog.
  kChangeTime,
  // Update values on the timezone settings page.
  kChangeTimezone,
};

// Parameters and callbacks for a security token PIN request that is to be shown
// to the user.
struct ASH_PUBLIC_EXPORT SecurityTokenPinRequest {
  SecurityTokenPinRequest();
  SecurityTokenPinRequest(SecurityTokenPinRequest&&);
  SecurityTokenPinRequest& operator=(SecurityTokenPinRequest&&);
  ~SecurityTokenPinRequest();

  // The user whose authentication triggered this PIN request.
  AccountId account_id;

  // Type of the code requested from the user.
  chromeos::SecurityTokenPinCodeType code_type =
      chromeos::SecurityTokenPinCodeType::kPin;

  // Whether the UI controls that allow user to enter the value should be
  // enabled. MUST be |false| when |attempts_left| is zero.
  bool enable_user_input = true;

  // An optional error to be displayed to the user.
  chromeos::SecurityTokenPinErrorLabel error_label =
      chromeos::SecurityTokenPinErrorLabel::kNone;

  // When non-negative, the UI should indicate this number to the user;
  // otherwise must be equal to -1.
  int attempts_left = -1;

  // Called when the user submits the input. Will not be called if the UI is
  // closed before that happens.
  base::OnceCallback<void(const std::string& user_input)> pin_entered_callback;

  // Called when the PIN request UI gets closed. Will not be called when the
  // browser itself requests the UI to be closed.
  base::OnceClosure pin_ui_closed_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_TYPES_H_
