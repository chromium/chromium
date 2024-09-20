// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/login/ui/lock_debug_view.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/curtain/remote_maintenance_curtain_view.h"
#include "ash/curtain/security_curtain_controller.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/auth_panel_debug_view.h"
#include "ash/login/ui/local_authentication_request_controller_impl.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/smartlock_state.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr const char* kDebugUserNames[] = {
    "Angelina Johnson", "Marcus Cohen", "Chris Wallace",
    "Debbie Craig",     "Stella Wong",  "Stephanie Wade",
};

constexpr const char* kDebugPublicAccountNames[] = {
    "Seattle Public Library",
    "San Jose Public Library",
    "Sunnyvale Public Library",
    "Mountain View Public Library",
};

constexpr const char* kDebugDetachableBases[] = {"Base A", "Base B", "Base C"};

constexpr const char kDebugOsVersion[] =
    "Chromium 64.0.3279.0 (Platform 10146.0.0 dev-channel peppy test)";
constexpr const char kDebugEnterpriseInfo[] = "Asset ID: 1111";
constexpr const char kDebugBluetoothName[] = "Bluetooth adapter";

constexpr const char kDebugKioskAppId[] = "asdf1234";
const AccountId kDebugKioskAppAccountId =
    AccountId::FromUserEmail("fake@email.com");
constexpr const char16_t kDebugKioskAppName[] = u"Test App Name";

constexpr const char kDebugDefaultLocaleCode[] = "en-GB";
constexpr const char kDebugDefaultLocaleTitle[] = "English";
constexpr const char kDebugEnterpriseDomain[] = "library.com";

enum class DebugAuthEnabledState {
  kAuthEnabled,

  // The auth disabled message is displayed because of unicorn account
  // restrictions.
  kTimeLimitOverride,
  kTimeUsageLimit,
  kTimeWindowLimit,

  // The auth disabled message is displayed because of multiprofile policy.
  // Note that this would only be displayed on the secondary login screen.
  kMultiUserPolicyPrimaryOnly,
  kMultiUserPolicyNotAllowed,

  // The auth disabled message is displayed because the force online
  // sign in is unavailable on the secondary login screen.
  // Note that this would only be displayed on the secondary login screen.
  kForceOnlineSignIn,

  kMaxValue = kForceOnlineSignIn,
};

// Additional state for a user that the debug UI needs to reference.
struct UserMetadata {
  explicit UserMetadata(const UserInfo& user_info)
      : account_id(user_info.account_id),
        display_name(user_info.display_name),
        type(user_info.type) {}

  AccountId account_id;
  std::string display_name;
  bool enable_password = true;
  bool enable_pin = false;
  bool pin_autosubmit = false;
  bool enable_tap_to_unlock = false;
  bool enable_challenge_response = false;  // Smart Card
  bool enable_auth = true;
  user_manager::UserType type = user_manager::UserType::kRegular;
  SmartLockState smart_lock_state = SmartLockState::kInactive;
  FingerprintState fingerprint_state = FingerprintState::UNAVAILABLE;
  DebugAuthEnabledState auth_enable_state = DebugAuthEnabledState::kAuthEnabled;
};

std::string DetachableBasePairingStatusToString(
    DetachableBasePairingStatus pairing_status) {
  switch (pairing_status) {
    case DetachableBasePairingStatus::kNone:
      return "No device";
    case DetachableBasePairingStatus::kAuthenticated:
      return "Authenticated";
    case DetachableBasePairingStatus::kNotAuthenticated:
      return "Not authenticated";
    case DetachableBasePairingStatus::kInvalidDevice:
      return "Invalid device";
  }
  return "Unknown";
}

// Update the user data based on |type| and |user_index|.
LoginUserInfo PopulateUserData(const LoginUserInfo& user,
                               user_manager::UserType type,
                               int user_index) {
  LoginUserInfo result = user;
  result.basic_user_info.type = type;

  bool is_public_account = type == user_manager::UserType::kPublicAccount;
  // Set debug user names and email. Useful for the stub user, which does not
  // have a name  and email set.
  result.basic_user_info.display_name =
      is_public_account
          ? kDebugPublicAccountNames[user_index %
                                     std::size(kDebugPublicAccountNames)]
          : kDebugUserNames[user_index % std::size(kDebugUserNames)];
  result.basic_user_info.display_email =
      result.basic_user_info.account_id.GetUserEmail();

  if (is_public_account) {
    result.public_account_info.emplace();
    result.public_account_info->device_enterprise_manager =
        kDebugEnterpriseDomain;
    result.public_account_info->default_locale = kDebugDefaultLocaleCode;
    result.public_account_info->show_expanded_view = true;

    std::vector<LocaleItem> locales;
    LocaleItem locale_item;
    locale_item.language_code = kDebugDefaultLocaleCode;
    locale_item.title = kDebugDefaultLocaleTitle;
    result.public_account_info->available_locales.push_back(
        std::move(locale_item));

    // Request keyboard layouts for the default locale.
    Shell::Get()
        ->login_screen_controller()
        ->RequestPublicSessionKeyboardLayouts(result.basic_user_info.account_id,
                                              kDebugDefaultLocaleCode);
  } else {
    result.public_account_info.reset();
  }

  return result;
}

std::unique_ptr<views::View> CreateCurtainOverlay() {
  return std::make_unique<ash::curtain::RemoteMaintenanceCurtainView>();
}

}  // namespace

// Applies a series of user-defined transformations to a
// |LoginDataDispatcher| instance; this is used for debugging and
// development. The debug overlay uses this class to change what data is exposed
// to the UI.
class LockDebugView::DebugDataDispatcherTransformer
    : public LoginDataDispatcher::Observer {
 public:
  DebugDataDispatcherTransformer(
      mojom::TrayActionState initial_lock_screen_note_state,
      LoginDataDispatcher* dispatcher,
      const base::RepeatingClosure& on_users_received,
      LockDebugView* lock_debug_view)
      : root_dispatcher_(dispatcher),
        lock_screen_note_state_(initial_lock_screen_note_state),
        on_users_received_(on_users_received),
        lock_debug_view_(lock_debug_view) {
    root_dispatcher_->AddObserver(this);
  }

  DebugDataDispatcherTransformer(const DebugDataDispatcherTransformer&) =
      delete;
  DebugDataDispatcherTransformer& operator=(
      const DebugDataDispatcherTransformer&) = delete;

  ~DebugDataDispatcherTransformer() override {
    auth_panel_debug_widget_ = nullptr;
    root_dispatcher_->RemoveObserver(this);
  }

  LoginDataDispatcher* debug_dispatcher() { return &debug_dispatcher_; }

  // Changes the number of displayed users to |count|.
  void SetUserCount(int count) { NotifyUsers(BuildUserList(count)); }

  // Create user list.
  std::vector<LoginUserInfo> BuildUserList(int count) {
    DCHECK(!root_users_.empty());

    count = std::max(count, 0);

    // Trim any extra debug users.
    if (debug_users_.size() > static_cast<size_t>(count)) {
      debug_users_.erase(debug_users_.begin() + count, debug_users_.end());
    }

    // Build |users|, add any new users to |debug_users|.
    std::vector<LoginUserInfo> users;
    for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
      users.push_back(root_users_[i % root_users_.size()]);
      if (i >= root_users_.size()) {
        users[i].basic_user_info.account_id = AccountId::FromUserEmailGaiaId(
            users[i].basic_user_info.account_id.GetUserEmail() +
                base::NumberToString(i),
            users[i].basic_user_info.account_id.GetGaiaId() +
                base::NumberToString(i));
      }

      // Setup user data based on the user type in debug_users_.
      user_manager::UserType type = (i < debug_users_.size())
                                        ? debug_users_[i].type
                                        : users[i].basic_user_info.type;
      users[i] = PopulateUserData(users[i], type, i);

      if (i >= debug_users_.size()) {
        debug_users_.push_back(UserMetadata(users[i].basic_user_info));
      }
    }

    return users;
  }

  void NotifyUsers(const std::vector<LoginUserInfo>& users) {
    // User notification resets PIN state.
    for (UserMetadata& user : debug_users_) {
      user.enable_pin = false;
    }

    debug_dispatcher_.SetUserList(users);
  }

  int GetUserCount() const { return debug_users_.size(); }

  std::u16string GetDisplayNameForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    return base::UTF8ToUTF16(debug_users_[user_index].display_name);
  }

  const AccountId& GetAccountIdForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    return debug_user->account_id;
  }

  // Activates or deactivates PIN for the user at |user_index|.
  void TogglePinStateForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    if (!debug_user->enable_pin) {
      debug_user->enable_pin = true;
      debug_user->pin_autosubmit = false;
      user_manager::KnownUser(Shell::Get()->local_state())
          .SetUserPinLength(debug_user->account_id, 0);
    } else if (!debug_user->pin_autosubmit) {
      debug_user->pin_autosubmit = true;
      user_manager::KnownUser(Shell::Get()->local_state())
          .SetUserPinLength(debug_user->account_id, 6);
    } else {
      debug_user->enable_pin = false;
      debug_user->pin_autosubmit = false;
      user_manager::KnownUser(Shell::Get()->local_state())
          .SetUserPinLength(debug_user->account_id, 0);
    }
    debug_dispatcher_.SetPinEnabledForUser(debug_user->account_id,
                                           debug_user->enable_pin,
                                           /*available_at*/ std::nullopt);
  }

  void ToggleDarkLigntModeForUserIndex(size_t user_index) {
    UserMetadata* debug_user = &debug_users_[user_index];
    user_manager::KnownUser(Shell::Get()->local_state())
        .SetBooleanPref(
            debug_user->account_id, prefs::kDarkModeEnabled,
            !ash::DarkLightModeController::Get()->IsDarkModeEnabled());
    Shell::Get()->login_screen_controller()->data_dispatcher()->NotifyFocusPod(
        debug_user->account_id);
  }

  // Activates or deactivates challenge response for the user at
  // |user_index|.
  void ToggleChallengeResponseStateForUserIndex(size_t user_index) {
    DCHECK(user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_user->enable_challenge_response =
        !debug_user->enable_challenge_response;
    debug_dispatcher_.SetChallengeResponseAuthEnabledForUser(
        debug_user->account_id, debug_user->enable_challenge_response);
  }

  // Activates or deactivates tap unlock for the user at |user_index|.
  void ToggleTapStateForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_user->enable_tap_to_unlock = !debug_user->enable_tap_to_unlock;
    debug_dispatcher_.SetTapToUnlockEnabledForUser(
        debug_user->account_id, debug_user->enable_tap_to_unlock);
  }

  // Enables click to auth for the user at |user_index|.
  void CycleSmartLockForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    // SmartLockState transition.
    auto get_next_state = [](SmartLockState state) {
      switch (state) {
        case SmartLockState::kInactive:
          return SmartLockState::kConnectingToPhone;
        case SmartLockState::kConnectingToPhone:
          return SmartLockState::kPhoneNotFound;
        case SmartLockState::kPhoneNotFound:
          return SmartLockState::kPhoneFoundLockedAndDistant;
        case SmartLockState::kPhoneFoundLockedAndDistant:
          return SmartLockState::kPhoneFoundUnlockedAndDistant;
        case SmartLockState::kPhoneFoundUnlockedAndDistant:
          return SmartLockState::kPhoneFoundLockedAndProximate;
        case SmartLockState::kPhoneFoundLockedAndProximate:
          return SmartLockState::kPhoneAuthenticated;
        case SmartLockState::kPhoneAuthenticated:
          return SmartLockState::kPhoneNotLockable;
        case SmartLockState::kPhoneNotLockable:
          return SmartLockState::kBluetoothDisabled;
        case SmartLockState::kBluetoothDisabled:
          return SmartLockState::kPhoneNotAuthenticated;
        case SmartLockState::kPhoneNotAuthenticated:
          return SmartLockState::kPrimaryUserAbsent;
        case SmartLockState::kPrimaryUserAbsent:
          return SmartLockState::kDisabled;
        case SmartLockState::kDisabled:
          return SmartLockState::kInactive;
      }
    };
    debug_user->smart_lock_state = get_next_state(debug_user->smart_lock_state);

    // Enable/disable click to unlock.
    debug_user->enable_tap_to_unlock =
        debug_user->smart_lock_state == SmartLockState::kPhoneAuthenticated;

    // Set Smart Lock state and enable/disable click to unlock.
    debug_dispatcher_.SetSmartLockState(debug_user->account_id,
                                        debug_user->smart_lock_state);

    // TODO(crbug.com/1233614): Remove this call once "Click to enter" button
    // no longer depends on user view tap.
    debug_dispatcher_.SetTapToUnlockEnabledForUser(
        debug_user->account_id, debug_user->enable_tap_to_unlock);
  }

  // Activates authentication request dialog for the user at |user_index|.
  void AuthRequestForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    const AccountId account_id = debug_user->account_id;

    std::unique_ptr<ash::UserContext> user_context =
        std::make_unique<ash::UserContext>(user_manager::UserType::kRegular,
                                           account_id);

    Shell::Get()->local_authentication_request_controller()->ShowWidget(
        base::BindOnce([](bool bla, std::unique_ptr<UserContext> ctx) {}),
        std::move(user_context));
  }

  // Activates AuthPanel for the user at |user_index|.
  void AuthPanelRequestForUserIndex(size_t user_index,
                                    bool use_legacy_authpanel) {
    if (auth_panel_debug_widget_) {
      LOG(ERROR) << "AuthPanelDebugWidget still exists.";
      return;
    }
    auto delegate = std::make_unique<views::DialogDelegate>();
    delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    delegate->SetModalType(ui::mojom::ModalType::kSystem);
    delegate->SetOwnedByWidget(true);
    delegate->SetCloseCallback(
        base::BindOnce(&LockDebugView::DebugDataDispatcherTransformer::
                           OnAuthPanelDebugWidgetClose,
                       base::Unretained(this)));

    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    const AccountId account_id = debug_user->account_id;
    delegate->SetContentsView(
        std::make_unique<AuthPanelDebugView>(account_id, use_legacy_authpanel));

    auth_panel_debug_widget_ = views::DialogDelegate::CreateDialogWidget(
        std::move(delegate),
        /*context=*/nullptr,
        /*parent=*/
        Shell::GetPrimaryRootWindow()->GetChildById(
            kShellWindowId_LockSystemModalContainer));
    auth_panel_debug_widget_->Show();
  }

  // Cycles fingerprint state for the user at |user_index|.
  void CycleFingerprintStateForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    debug_user->fingerprint_state = static_cast<FingerprintState>(
        (static_cast<int>(debug_user->fingerprint_state) + 1) %
        (static_cast<int>(FingerprintState::kMaxValue) + 1));
    debug_dispatcher_.SetFingerprintState(debug_user->account_id,
                                          debug_user->fingerprint_state);
  }

  void AuthenticateSmartLockForUserIndex(size_t user_index, bool success) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_dispatcher_.NotifySmartLockAuthResult(debug_user->account_id,
                                                success);
  }

  void AuthenticateFingerprintForUserIndex(size_t user_index, bool success) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_dispatcher_.NotifyFingerprintAuthResult(debug_user->account_id,
                                                  success);
  }

  // Toggles force online sign-in for the user at |user_index|.
  void ToggleForceOnlineSignInForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    LockContentsViewTestApi lock_test_api(lock_debug_view_->lock());
    lock_test_api.ToggleForceOnlineSignInForUser(
        debug_users_[user_index].account_id);
  }

  // Enables or disables user management for the user at |user_index|.
  void ToggleManagementForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    LockContentsViewTestApi lock_test_api(lock_debug_view_->lock());
    lock_test_api.ToggleManagementForUser(debug_users_[user_index].account_id);
  }

  // Toggles TPM disabled message for the user at |user_index|.
  void ToggleDisableTpmForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    LockContentsViewTestApi lock_test_api(lock_debug_view_->lock());
    lock_test_api.ToggleDisableTpmForUser(debug_users_[user_index].account_id);
  }

  // Cycles disabled auth message for the user at |user_index|.
  void CycleDisabledAuthMessageForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    debug_user->auth_enable_state = static_cast<DebugAuthEnabledState>(
        (static_cast<int>(debug_user->auth_enable_state) + 1) %
        (static_cast<int>(DebugAuthEnabledState::kMaxValue) + 1));

    debug_user->enable_auth = true;
    AuthDisabledReason reason;
    user_manager::MultiUserSignInPolicy multi_user_sign_in_policy =
        user_manager::MultiUserSignInPolicy::kUnrestricted;

    switch (debug_user->auth_enable_state) {
      case DebugAuthEnabledState::kAuthEnabled:
        debug_user->enable_auth = true;
        break;
      case DebugAuthEnabledState::kTimeLimitOverride:
        reason = AuthDisabledReason::kTimeLimitOverride;
        break;
      case DebugAuthEnabledState::kTimeUsageLimit:
        reason = AuthDisabledReason::kTimeUsageLimit;
        break;
      case DebugAuthEnabledState::kTimeWindowLimit:
        reason = AuthDisabledReason::kTimeWindowLimit;
        break;
      case DebugAuthEnabledState::kMultiUserPolicyPrimaryOnly:
        multi_user_sign_in_policy =
            user_manager::MultiUserSignInPolicy::kPrimaryOnly;
        break;
      case DebugAuthEnabledState::kMultiUserPolicyNotAllowed:
        multi_user_sign_in_policy =
            user_manager::MultiUserSignInPolicy::kNotAllowed;
        break;
      case DebugAuthEnabledState::kForceOnlineSignIn:
        break;
    }

    debug_dispatcher_.EnableAuthForUser(debug_user->account_id);
    LockContentsViewTestApi lock_test_api(lock_debug_view_->lock());
    lock_test_api.SetMultiUserSignInPolicyForUser(
        debug_users_[user_index].account_id, multi_user_sign_in_policy);
    lock_test_api.UndoForceOnlineSignInForUser(
        debug_users_[user_index].account_id);

    switch (debug_user->auth_enable_state) {
      case DebugAuthEnabledState::kAuthEnabled:
        break;
      case DebugAuthEnabledState::kTimeLimitOverride:
      case DebugAuthEnabledState::kTimeUsageLimit:
      case DebugAuthEnabledState::kTimeWindowLimit:
        debug_dispatcher_.DisableAuthForUser(
            debug_user->account_id,
            AuthDisabledData(
                reason,
                base::Time::Now() + base::Hours(user_index) + base::Hours(8),
                base::Minutes(15), true /*bool disable_lock_screen_media*/));
        break;
      case DebugAuthEnabledState::kMultiUserPolicyPrimaryOnly:
      case DebugAuthEnabledState::kMultiUserPolicyNotAllowed:
        lock_test_api.SetMultiUserSignInPolicyForUser(
            debug_users_[user_index].account_id, multi_user_sign_in_policy);
        break;
      case DebugAuthEnabledState::kForceOnlineSignIn:
        debug_dispatcher_.ForceOnlineSignInForUser(
            debug_users_[user_index].account_id);
        break;
    }
  }

  // Convert user type to regular user or public account for the user at
  // |user_index|.
  void TogglePublicAccountForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata& user = debug_users_[user_index];
    // Swap the type between regular and public account.
    user.type = user.type == user_manager::UserType::kRegular
                    ? user_manager::UserType::kPublicAccount
                    : user_manager::UserType::kRegular;

    std::vector<LoginUserInfo> users = BuildUserList(debug_users_.size());
    // Update display name and email in debug users.
    debug_users_[user_index] = UserMetadata(users[user_index].basic_user_info);
    NotifyUsers(std::move(users));
  }

  void ToggleLockScreenNoteButton() {
    if (lock_screen_note_state_ == mojom::TrayActionState::kAvailable) {
      lock_screen_note_state_ = mojom::TrayActionState::kNotAvailable;
    } else {
      lock_screen_note_state_ = mojom::TrayActionState::kAvailable;
    }

    debug_dispatcher_.SetLockScreenNoteState(lock_screen_note_state_);
  }

  void AddKioskApp(ShelfWidget* shelf_widget) {
    kiosk_apps_.emplace_back(KioskAppMenuEntry::AppType::kChromeApp,
                             kDebugKioskAppAccountId, kDebugKioskAppId,
                             kDebugKioskAppName, gfx::ImageSkia());
    shelf_widget->GetLoginShelfView()->SetKioskApps(kiosk_apps_);
  }

  void RemoveKioskApp(ShelfWidget* shelf_widget) {
    if (kiosk_apps_.empty()) {
      return;
    }
    kiosk_apps_.pop_back();
    shelf_widget->GetLoginShelfView()->SetKioskApps(kiosk_apps_);
  }

  void AddSystemInfo(const std::string& os_version,
                     const std::string& enterprise_info,
                     const std::string& bluetooth_name,
                     bool adb_sideloading_enabled) {
    debug_dispatcher_.SetSystemInfo(true /*show*/, false /*enforced*/,
                                    os_version, enterprise_info, bluetooth_name,
                                    adb_sideloading_enabled);
  }

  void UpdateWarningMessage(const std::u16string& message) {
    debug_dispatcher_.UpdateWarningMessage(message);
  }

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override {
    // Update root_users_ to new source data.
    root_users_.clear();
    for (auto& user : users) {
      root_users_.push_back(user);
    }

    // Rebuild debug users using new source data.
    SetUserCount(root_users_.size());

    on_users_received_.Run();
  }
  void OnUserAuthFactorsChanged(
      const AccountId& user,
      cryptohome::AuthFactorsSet auth_factors,
      cryptohome::PinLockAvailability pin_available_at) override {
    // Forward notification only if the user is currently being shown.
    for (auto& debug_user : debug_users_) {
      if (debug_user.account_id == user) {
        debug_user.enable_password =
            auth_factors.Has(cryptohome::AuthFactorType::kPassword);
        debug_user.enable_pin =
            auth_factors.Has(cryptohome::AuthFactorType::kPin);
        debug_user.enable_challenge_response =
            auth_factors.Has(cryptohome::AuthFactorType::kSmartCard);
        debug_dispatcher_.SetAuthFactorsForUser(user, auth_factors,
                                                pin_available_at);
        break;
      }
    }
  }
  void OnPinEnabledForUserChanged(
      const AccountId& user,
      bool enabled,
      cryptohome::PinLockAvailability available_at) override {
    // Forward notification only if the user is currently being shown.
    for (auto& debug_user : debug_users_) {
      if (debug_user.account_id == user) {
        debug_user.enable_pin = enabled;
        debug_dispatcher_.SetPinEnabledForUser(user, enabled, available_at);
        break;
      }
    }
  }
  void OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                          bool enabled) override {
    // Forward notification only if the user is currently being shown.
    for (auto& debug_user : debug_users_) {
      if (debug_user.account_id == user) {
        debug_user.enable_tap_to_unlock = enabled;
        debug_dispatcher_.SetTapToUnlockEnabledForUser(user, enabled);
        break;
      }
    }
  }
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override {
    lock_screen_note_state_ = state;
    debug_dispatcher_.SetLockScreenNoteState(state);
  }
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override {
    debug_dispatcher_.SetDetachableBasePairingStatus(pairing_status);
  }

  void OnPublicSessionKeyboardLayoutsChanged(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<InputMethodItem>& keyboard_layouts) override {
    debug_dispatcher_.SetPublicSessionKeyboardLayouts(account_id, locale,
                                                      keyboard_layouts);
  }

  void OnPublicSessionShowFullManagementDisclosureChanged(
      bool show_full_management_disclosure) override {
    debug_dispatcher_.SetPublicSessionShowFullManagementDisclosure(
        show_full_management_disclosure);
  }

  void OnAuthPanelDebugWidgetClose() { auth_panel_debug_widget_ = nullptr; }

 private:
  // The debug overlay UI takes ground-truth data from |root_dispatcher_|,
  // applies a series of transformations to it, and exposes it to the UI via
  // |debug_dispatcher_|.
  raw_ptr<LoginDataDispatcher> root_dispatcher_;  // Unowned.
  LoginDataDispatcher debug_dispatcher_;

  // Original set of users from |root_dispatcher_|.
  std::vector<LoginUserInfo> root_users_;

  // Metadata for users that the UI is displaying.
  std::vector<UserMetadata> debug_users_;

  // The current lock screen note action state.
  mojom::TrayActionState lock_screen_note_state_;

  // List of kiosk apps loaded.
  std::vector<KioskAppMenuEntry> kiosk_apps_;

  // Called when a new user list has been received.
  base::RepeatingClosure on_users_received_;

  raw_ptr<views::Widget> auth_panel_debug_widget_ = nullptr;

  // Called for testing functions not belonging to the login data dispatcher.
  // In such a case, we want to bypass the event handling mechanism and do
  // direct calls to the lock screen. We need either an instance of
  // LockDebugView or LockContentsView in order to do so.
  const raw_ptr<LockDebugView> lock_debug_view_;
};

// In-memory wrapper around LoginDetachableBaseModel used by lock UI.
// It provides, methods to override the detachable base pairing state seen by
// the UI.
class LockDebugView::DebugLoginDetachableBaseModel
    : public LoginDetachableBaseModel {
 public:
  static constexpr int kNullBaseId = -1;

  DebugLoginDetachableBaseModel() = default;

  DebugLoginDetachableBaseModel(const DebugLoginDetachableBaseModel&) = delete;
  DebugLoginDetachableBaseModel& operator=(
      const DebugLoginDetachableBaseModel&) = delete;

  ~DebugLoginDetachableBaseModel() override = default;

  bool debugging_pairing_state() const { return pairing_status_.has_value(); }

  // Calculates the pairing status to which the model should be changed when
  // button for cycling detachable base pairing statuses is clicked.
  DetachableBasePairingStatus NextPairingStatus() const {
    if (!pairing_status_.has_value()) {
      return DetachableBasePairingStatus::kNone;
    }

    switch (*pairing_status_) {
      case DetachableBasePairingStatus::kNone:
        return DetachableBasePairingStatus::kAuthenticated;
      case DetachableBasePairingStatus::kAuthenticated:
        return DetachableBasePairingStatus::kNotAuthenticated;
      case DetachableBasePairingStatus::kNotAuthenticated:
        return DetachableBasePairingStatus::kInvalidDevice;
      case DetachableBasePairingStatus::kInvalidDevice:
        return DetachableBasePairingStatus::kNone;
    }

    return DetachableBasePairingStatus::kNone;
  }

  // Calculates the debugging detachable base ID that should become the paired
  // base in the model when the button for cycling paired bases is clicked.
  int NextBaseId() const {
    return (base_id_ + 1) % std::size(kDebugDetachableBases);
  }

  // Gets the descripting text for currently paired base, if any.
  std::string BaseButtonText() const {
    if (base_id_ < 0) {
      return "No base";
    }
    return kDebugDetachableBases[base_id_];
  }

  // Sets the model's pairing state - base pairing status, and the currently
  // paired base ID. ID should be an index in |kDebugDetachableBases| array, and
  // it should be set if pairing status is kAuthenticated. The base ID is
  // ignored if pairing state is different than kAuthenticated.
  void SetPairingState(DetachableBasePairingStatus pairing_status,
                       int base_id) {
    pairing_status_ = pairing_status;
    if (pairing_status == DetachableBasePairingStatus::kAuthenticated) {
      CHECK_GE(base_id, 0);
      CHECK_LT(base_id, static_cast<int>(std::size(kDebugDetachableBases)));
      base_id_ = base_id;
    } else {
      base_id_ = kNullBaseId;
    }

    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(pairing_status);
  }

  // Marks the paired base (as seen by the model) as the user's last used base.
  // No-op if the current pairing status is different than kAuthenticated.
  void SetBaseLastUsedForUser(const AccountId& account_id) {
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated) {
      return;
    }
    DCHECK_GE(base_id_, 0);

    last_used_bases_[account_id] = base_id_;
    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(*pairing_status_);
  }

  // Clears all in-memory pairing state.
  void ClearDebugPairingState() {
    pairing_status_ = std::nullopt;
    base_id_ = kNullBaseId;
    last_used_bases_.clear();

    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(DetachableBasePairingStatus::kNone);
  }

  // LoginDetachableBaseModel:
  DetachableBasePairingStatus GetPairingStatus() override {
    if (!pairing_status_.has_value()) {
      return DetachableBasePairingStatus::kNone;
    }
    return *pairing_status_;
  }
  bool PairedBaseMatchesLastUsedByUser(const UserInfo& user_info) override {
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated) {
      return false;
    }

    if (last_used_bases_.count(user_info.account_id) == 0) {
      return true;
    }
    return last_used_bases_[user_info.account_id] == base_id_;
  }
  bool SetPairedBaseAsLastUsedByUser(const UserInfo& user_info) override {
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated) {
      return false;
    }

    last_used_bases_[user_info.account_id] = base_id_;
    return true;
  }

 private:
  // In-memory detachable base pairing state.
  std::optional<DetachableBasePairingStatus> pairing_status_;
  int base_id_ = kNullBaseId;
  // Maps user account to the last used detachable base ID (base ID being the
  // base's index in kDebugDetachableBases array).
  std::map<AccountId, int> last_used_bases_;
};

LockDebugView::LockDebugView(mojom::TrayActionState initial_note_action_state,
                             LockScreen::ScreenType screen_type)
    : debug_data_dispatcher_(std::make_unique<DebugDataDispatcherTransformer>(
          initial_note_action_state,
          Shell::Get()->login_screen_controller()->data_dispatcher(),
          base::BindRepeating(
              &LockDebugView::UpdatePerUserActionContainerAndLayout,
              base::Unretained(this)),
          this)),
      next_auth_error_type_(AuthErrorType::kFirstUnlockFailed) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  auto debug_detachable_base_model =
      std::make_unique<DebugLoginDetachableBaseModel>();
  debug_detachable_base_model_ = debug_detachable_base_model.get();

  lock_ = new LockContentsView(initial_note_action_state, screen_type,
                               debug_data_dispatcher_->debug_dispatcher(),
                               std::move(debug_detachable_base_model));
  AddChildView(lock_.get());

  container_ = new NonAccessibleView();
  container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(container_.get());

  auto* margin = new NonAccessibleView();
  margin->SetPreferredSize(gfx::Size(10, 10));
  container_->AddChildView(margin);

  global_action_view_container_ = new NonAccessibleView();
  global_action_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  auto add_horizontal_container = [&]() {
    auto* container = new NonAccessibleView();
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    global_action_view_container_->AddChildView(container);
    return container;
  };

  auto* change_users_container = add_horizontal_container();
  AddButton("Add user",
            base::BindRepeating(&LockDebugView::AddOrRemoveUsersButtonPressed,
                                base::Unretained(this), 1),
            change_users_container);
  AddButton("Add 10 users",
            base::BindRepeating(&LockDebugView::AddOrRemoveUsersButtonPressed,
                                base::Unretained(this), 10),
            change_users_container);
  AddButton("Remove user",
            base::BindRepeating(&LockDebugView::AddOrRemoveUsersButtonPressed,
                                base::Unretained(this), -1),
            change_users_container);

  auto* login_ui_components_container = add_horizontal_container();
  AddButton("Show AuthInputRowView",
            base::BindRepeating(&LockDebugView::AuthInputRowView,
                                base::Unretained(this)),
            login_ui_components_container);

  auto* toggle_container = add_horizontal_container();
  AddButton("Blur", base::BindRepeating([]() {
              auto* const wallpaper_controller =
                  Shell::Get()->wallpaper_controller();
              wallpaper_controller->UpdateWallpaperBlurForLockState(
                  !wallpaper_controller->IsWallpaperBlurredForLockState());
            }),
            toggle_container);
  AddButton("Toggle note action",
            base::BindRepeating(
                &DebugDataDispatcherTransformer::ToggleLockScreenNoteButton,
                base::Unretained(debug_data_dispatcher_.get())),
            toggle_container);
  AddButton("Toggle caps lock", base::BindRepeating([]() {
              ImeControllerImpl* ime_controller =
                  Shell::Get()->ime_controller();
              ime_controller->SetCapsLockEnabled(
                  !ime_controller->IsCapsLockEnabled());
            }),
            toggle_container);
  global_action_add_system_info_ =
      AddButton("Add system info",
                base::BindRepeating(&LockDebugView::AddSystemInfoButtonPressed,
                                    base::Unretained(this)),
                toggle_container);
  global_action_toggle_auth_ =
      AddButton("Auth (allowed)",
                base::BindRepeating(&LockDebugView::ToggleAuthButtonPressed,
                                    base::Unretained(this)),
                toggle_container);
  AddButton("Cycle auth error",
            base::BindRepeating(&LockDebugView::CycleAuthErrorMessage,
                                base::Unretained(this)),
            toggle_container);
  AddButton(
      "Toggle warning banner",
      base::BindRepeating(&LockDebugView::ToggleWarningBannerButtonPressed,
                          base::Unretained(this)),
      toggle_container);
  AddButton("Show parent access",
            base::BindRepeating(&LockContentsView::ShowParentAccessDialog,
                                base::Unretained(lock_)),
            toggle_container);

  auto* kiosk_container = add_horizontal_container();
  AddButton("Add kiosk app",
            base::BindRepeating(&LockDebugView::AddKioskAppButtonPressed,
                                base::Unretained(this)),
            kiosk_container);
  AddButton("Remove kiosk app",
            base::BindRepeating(&LockDebugView::RemoveKioskAppButtonPressed,
                                base::Unretained(this)),
            kiosk_container);
  AddButton("Show kiosk error",
            base::BindRepeating(
                &LoginScreenController::ShowKioskAppError,
                base::Unretained(Shell::Get()->login_screen_controller()),
                "Test error message."),
            kiosk_container);

  auto* managed_sessions_container = add_horizontal_container();
  AddButton("Toggle managed session disclosure",
            base::BindRepeating(
                &LockDebugView::ToggleManagedSessionDisclosureButtonPressed,
                base::Unretained(this)),
            managed_sessions_container);

  AddButton("Show security curtain screen",
            base::BindRepeating(
                &LockDebugView::ShowSecurityCurtainScreenButtonPressed,
                base::Unretained(this)),
            kiosk_container);

  global_action_detachable_base_group_ = add_horizontal_container();
  UpdateDetachableBaseColumn();

  per_user_action_view_container_ = new NonAccessibleView();
  per_user_action_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  UpdatePerUserActionContainer();

  auto make_scroll = [](views::View* content,
                        int height) -> std::unique_ptr<views::View> {
    std::unique_ptr<views::ScrollView> scroll =
        views::ScrollView::CreateScrollViewWithBorder();
    scroll->SetPreferredSize(gfx::Size(600, height));
    scroll->SetContents(base::WrapUnique(content));
    scroll->SetBackgroundColor(std::nullopt);
    scroll->SetVerticalScrollBar(std::make_unique<views::OverlayScrollBar>(
        views::ScrollBar::Orientation::kVertical));
    scroll->SetHorizontalScrollBar(std::make_unique<views::OverlayScrollBar>(
        views::ScrollBar::Orientation::kHorizontal));
    return scroll;
  };
  container_->AddChildView(make_scroll(global_action_view_container_, 110));
  container_->AddChildView(make_scroll(per_user_action_view_container_, 100));

  DeprecatedLayoutImmediately();
}

LockDebugView::~LockDebugView() {
  // Make sure debug_data_dispatcher_ lives longer than LockContentsView so
  // pointer debug_dispatcher_ is always valid for LockContentsView.
  delete lock_;
}

void LockDebugView::Layout(PassKey) {
  global_action_view_container_->SizeToPreferredSize();
  per_user_action_view_container_->SizeToPreferredSize();

  LayoutSuperclass<views::View>(this);

  lock_->SetBoundsRect(GetLocalBounds());
  container_->SetPosition(gfx::Point());
  container_->SizeToPreferredSize();

  for (views::View* child : container_->children()) {
    child->DeprecatedLayoutImmediately();
  }
}

void LockDebugView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdatePerUserActionContainerAndLayout();
}

void LockDebugView::AddOrRemoveUsersButtonPressed(int delta) {
  debug_data_dispatcher_->SetUserCount(
      std::max(0, debug_data_dispatcher_->GetUserCount() + delta));
  UpdatePerUserActionContainer();
  DeprecatedLayoutImmediately();
}

void LockDebugView::AddSystemInfoButtonPressed() {
  ++num_system_info_clicks_;
  if (num_system_info_clicks_ >= 7) {
    global_action_add_system_info_->SetEnabled(false);
  }

  std::string os_version = num_system_info_clicks_ / 4 ? kDebugOsVersion : "";
  std::string enterprise_info =
      (num_system_info_clicks_ % 4) / 2 ? kDebugEnterpriseInfo : "";
  std::string bluetooth_name =
      num_system_info_clicks_ % 2 ? kDebugBluetoothName : "";
  bool adb_sideloading_enabled = num_system_info_clicks_ % 3;
  debug_data_dispatcher_->AddSystemInfo(
      os_version, enterprise_info, bluetooth_name, adb_sideloading_enabled);
}

void LockDebugView::ToggleAuthButtonPressed() {
  auto get_next_auth_state = [](LoginScreenController::ForceFailAuth auth) {
    switch (auth) {
      case LoginScreenController::ForceFailAuth::kOff:
        return LoginScreenController::ForceFailAuth::kImmediate;
      case LoginScreenController::ForceFailAuth::kImmediate:
        return LoginScreenController::ForceFailAuth::kDelayed;
      case LoginScreenController::ForceFailAuth::kDelayed:
        return LoginScreenController::ForceFailAuth::kOff;
    }
  };
  auto get_auth_label = [](LoginScreenController::ForceFailAuth auth) {
    switch (auth) {
      case LoginScreenController::ForceFailAuth::kOff:
        return "Auth (allowed)";
      case LoginScreenController::ForceFailAuth::kImmediate:
        return "Auth (immediate fail)";
      case LoginScreenController::ForceFailAuth::kDelayed:
        return "Auth (delayed fail)";
    }
  };
  force_fail_auth_ = get_next_auth_state(force_fail_auth_);
  global_action_toggle_auth_->SetText(
      base::ASCIIToUTF16(get_auth_label(force_fail_auth_)));
  DeprecatedLayoutImmediately();
  Shell::Get()
      ->login_screen_controller()
      ->set_force_fail_auth_for_debug_overlay(force_fail_auth_);
}

void LockDebugView::AuthInputRowView() {
  if (auth_input_row_debug_widget_) {
    LOG(ERROR) << "AuthInputRowWidget still exists.";
    return;
  }
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  delegate->SetOwnedByWidget(true);
  delegate->SetCloseCallback(base::BindOnce(
      &LockDebugView::OnAuthInputRowDebugWidgetClose, base::Unretained(this)));

  auto container_view = std::make_unique<views::View>();

  auto* layout =
      container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  container_view->SetPreferredSize(gfx::Size({500, 400}));

  container_view->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, 0));

  container_view->AddChildView(std::make_unique<ash::AuthInputRowView>(
      ash::AuthInputRowView::AuthType::kPassword));

  delegate->SetContentsView(std::move(container_view));

  auth_input_row_debug_widget_ = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate),
      /*context=*/nullptr,
      /*parent=*/
      Shell::GetPrimaryRootWindow()->GetChildById(
          kShellWindowId_LockSystemModalContainer));
  auth_input_row_debug_widget_->Show();
}

void LockDebugView::OnAuthInputRowDebugWidgetClose() {
  auth_input_row_debug_widget_ = nullptr;
}

void LockDebugView::AddKioskAppButtonPressed() {
  debug_data_dispatcher_->AddKioskApp(
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
}

void LockDebugView::RemoveKioskAppButtonPressed() {
  debug_data_dispatcher_->RemoveKioskApp(
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
}

void LockDebugView::ToggleDebugDetachableBaseButtonPressed() {
  if (debug_detachable_base_model_->debugging_pairing_state()) {
    debug_detachable_base_model_->ClearDebugPairingState();
    // In authenticated state, per user column has a button to mark the
    // current base as last used for the user - it should get removed when the
    // detachable base debugging gets disabled.
    UpdatePerUserActionContainer();
  } else {
    debug_detachable_base_model_->SetPairingState(
        DetachableBasePairingStatus::kNone,
        DebugLoginDetachableBaseModel::kNullBaseId);
  }
  UpdateDetachableBaseColumn();
  DeprecatedLayoutImmediately();
}

void LockDebugView::CycleDetachableBaseStatusButtonPressed() {
  debug_detachable_base_model_->SetPairingState(
      debug_detachable_base_model_->NextPairingStatus(),
      debug_detachable_base_model_->NextBaseId());
  UpdatePerUserActionContainer();
  UpdateDetachableBaseColumn();
  DeprecatedLayoutImmediately();
}

void LockDebugView::CycleDetachableBaseIdButtonPressed() {
  debug_detachable_base_model_->SetPairingState(
      DetachableBasePairingStatus::kAuthenticated,
      debug_detachable_base_model_->NextBaseId());
  UpdateDetachableBaseColumn();
  DeprecatedLayoutImmediately();
}

void LockDebugView::ToggleWarningBannerButtonPressed() {
  debug_data_dispatcher_->UpdateWarningMessage(
      is_warning_banner_shown_
          ? std::u16string()
          : u"A critical update is ready to install. Sign in to get started.");
  is_warning_banner_shown_ = !is_warning_banner_shown_;
}

void LockDebugView::ToggleManagedSessionDisclosureButtonPressed() {
  is_managed_session_disclosure_shown_ = !is_managed_session_disclosure_shown_;
  debug_data_dispatcher_->OnPublicSessionShowFullManagementDisclosureChanged(
      is_managed_session_disclosure_shown_);
}

void LockDebugView::ShowSecurityCurtainScreenButtonPressed() {
  auto& controller = ash::Shell::Get()->security_curtain_controller();

  // We don't support toggling this on and off, since once you are in the
  // curtain screen there is no way to leave it (by design).
  ash::curtain::SecurityCurtainController::InitParams params{
      /*curtain_factory=*/base::BindRepeating(CreateCurtainOverlay)};
  controller.Enable(params);
}

void LockDebugView::UseDetachableBaseButtonPressed(int index) {
  debug_detachable_base_model_->SetBaseLastUsedForUser(
      debug_data_dispatcher_->GetAccountIdForUserIndex(index));
}

void LockDebugView::TogglePublicAccountButtonPressed(int index) {
  debug_data_dispatcher_->TogglePublicAccountForUserIndex(index);
  UpdatePerUserActionContainer();
  DeprecatedLayoutImmediately();
}

void LockDebugView::CycleAuthErrorMessage() {
  LockContentsViewTestApi lock_test_api(lock_);
  switch (next_auth_error_type_) {
    case AuthErrorType::kFirstUnlockFailed:
      next_auth_error_type_ = AuthErrorType::kFirstUnlockFailedCapsLockOn;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          false /*caps_enabled*/);
      debug_detachable_base_model_->SetPairingState(
          DetachableBasePairingStatus::kNone,
          DebugLoginDetachableBaseModel::kNullBaseId);
      lock_test_api.ShowAuthErrorBubble(1);
      return;
    case AuthErrorType::kFirstUnlockFailedCapsLockOn:
      next_auth_error_type_ = AuthErrorType::kSecondUnlockFailed;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          true /*caps_enabled*/);
      lock_test_api.ShowAuthErrorBubble(1);
      return;
    case AuthErrorType::kSecondUnlockFailed:
      next_auth_error_type_ = AuthErrorType::kSecondUnlockFailedCapsLockOn;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          false /*caps_enabled*/);
      lock_test_api.ShowAuthErrorBubble(2);
      return;
    case AuthErrorType::kSecondUnlockFailedCapsLockOn:
      next_auth_error_type_ = AuthErrorType::kDetachableBaseFailed;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          true /*caps_enabled*/);
      lock_test_api.ShowAuthErrorBubble(2);
      return;
    case AuthErrorType::kDetachableBaseFailed:
      next_auth_error_type_ = AuthErrorType::kFirstUnlockFailed;
      debug_detachable_base_model_->SetPairingState(
          DetachableBasePairingStatus::kNotAuthenticated,
          DebugLoginDetachableBaseModel::kNullBaseId);
      return;
    default:
      NOTREACHED();
  }
}

void LockDebugView::UpdatePerUserActionContainer() {
  per_user_action_view_container_->RemoveAllChildViews();

  int num_users = debug_data_dispatcher_->GetUserCount();
  for (int i = 0; i < num_users; ++i) {
    auto* row = new NonAccessibleView();
    row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    auto* name = new views::Label();
    name->SetText(debug_data_dispatcher_->GetDisplayNameForUserIndex(i));
    name->SetSubpixelRenderingEnabled(false);
    name->SetEnabledColorId(kColorAshTextColorPrimary);
    name->SetAutoColorReadabilityEnabled(false);
    row->AddChildView(name);

    AddButton("Toggle PIN",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::TogglePinStateForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);
    AddButton(
        "Toggle Dark/Light mode",
        base::BindRepeating(
            &DebugDataDispatcherTransformer::ToggleDarkLigntModeForUserIndex,
            base::Unretained(debug_data_dispatcher_.get()), i),
        row);

    AddButton(
        "Toggle Smart card",
        base::BindRepeating(&DebugDataDispatcherTransformer::
                                ToggleChallengeResponseStateForUserIndex,
                            base::Unretained(debug_data_dispatcher_.get()), i),
        row);
    AddButton("Toggle Tap",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::ToggleTapStateForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);
    AddButton("Cycle Smart Lock",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::CycleSmartLockForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);
    for (bool success : {true, false}) {
      std::string button_label = "Send Smart Lock auth ";
      button_label += (success ? "success" : "fail");
      AddButton(std::move(button_label),
                base::BindRepeating(
                    &DebugDataDispatcherTransformer::
                        AuthenticateSmartLockForUserIndex,
                    base::Unretained(debug_data_dispatcher_.get()), i, success),
                row);
    }
    AddButton(
        "Cycle fingerprint state",
        base::BindRepeating(
            &DebugDataDispatcherTransformer::CycleFingerprintStateForUserIndex,
            base::Unretained(debug_data_dispatcher_.get()), i),
        row);
    AddButton("Send fingerprint auth success",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::
                      AuthenticateFingerprintForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i, true),
              row);
    AddButton("Send fingerprint auth fail",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::
                      AuthenticateFingerprintForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i, false),
              row);
    AddButton(
        "Toggle force online sign-in",
        base::BindRepeating(&DebugDataDispatcherTransformer::
                                ToggleForceOnlineSignInForUserIndex,
                            base::Unretained(debug_data_dispatcher_.get()), i),
        row);
    AddButton("Toggle user is managed",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::ToggleManagementForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);
    AddButton("Toggle disabled TPM",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::ToggleDisableTpmForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);
    AddButton(
        "Cycle disabled auth",
        base::BindRepeating(&DebugDataDispatcherTransformer::
                                CycleDisabledAuthMessageForUserIndex,
                            base::Unretained(debug_data_dispatcher_.get()), i),
        row);

    AddButton("Show legacy AuthPanel",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::AuthPanelRequestForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i,
                  /*use_legacy_authpanel=*/true),
              row);

    AddButton("Show AuthPanel",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::AuthPanelRequestForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i,
                  /*use_legacy_authpanel=*/false),
              row);

    AddButton("Show local authentication request",
              base::BindRepeating(
                  &DebugDataDispatcherTransformer::AuthRequestForUserIndex,
                  base::Unretained(debug_data_dispatcher_.get()), i),
              row);

    if (debug_detachable_base_model_->debugging_pairing_state() &&
        debug_detachable_base_model_->GetPairingStatus() ==
            DetachableBasePairingStatus::kAuthenticated) {
      AddButton(
          "Set base used",
          base::BindRepeating(&LockDebugView::UseDetachableBaseButtonPressed,
                              base::Unretained(this), i),
          row);
    }

    AddButton(
        "Toggle Public Account",
        base::BindRepeating(&LockDebugView::TogglePublicAccountButtonPressed,
                            base::Unretained(this), i),
        row)
        ->set_tag(i);

    per_user_action_view_container_->AddChildView(row);
  }
}

void LockDebugView::UpdatePerUserActionContainerAndLayout() {
  UpdatePerUserActionContainer();
  DeprecatedLayoutImmediately();
}

void LockDebugView::UpdateDetachableBaseColumn() {
  global_action_detachable_base_group_->RemoveAllChildViews();

  AddButton("Debug detachable base",
            base::BindRepeating(
                &LockDebugView::ToggleDebugDetachableBaseButtonPressed,
                base::Unretained(this)),
            global_action_detachable_base_group_);
  if (!debug_detachable_base_model_->debugging_pairing_state()) {
    return;
  }

  const std::string kPairingStatusText =
      "Pairing status: " +
      DetachableBasePairingStatusToString(
          debug_detachable_base_model_->GetPairingStatus());
  AddButton(kPairingStatusText,
            base::BindRepeating(
                &LockDebugView::CycleDetachableBaseStatusButtonPressed,
                base::Unretained(this)),
            global_action_detachable_base_group_);

  views::LabelButton* cycle_detachable_base_id = AddButton(
      debug_detachable_base_model_->BaseButtonText(),
      base::BindRepeating(&LockDebugView::CycleDetachableBaseIdButtonPressed,
                          base::Unretained(this)),
      global_action_detachable_base_group_);
  bool base_authenticated = debug_detachable_base_model_->GetPairingStatus() ==
                            DetachableBasePairingStatus::kAuthenticated;
  cycle_detachable_base_id->SetEnabled(base_authenticated);
}

views::LabelButton* LockDebugView::AddButton(
    const std::string& text,
    views::Button::PressedCallback callback,
    views::View* container) {
  // Creates a button with |text| that cannot be focused.
  auto button = std::make_unique<views::MdTextButton>(std::move(callback),
                                                      base::ASCIIToUTF16(text));
  button->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  views::LabelButton* view = button.get();
  container->AddChildView(
      login_views_utils::WrapViewForPreferredSize(std::move(button)));
  return view;
}

}  // namespace ash
