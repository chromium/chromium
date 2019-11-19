// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_debug_view.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/ime/ime_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

namespace ButtonId {
enum {
  kGlobalAddUser = 1,
  kGlobalAddTenUsers,
  kGlobalRemoveUser,
  kGlobalToggleBlur,
  kGlobalToggleNoteAction,
  kGlobalToggleCapsLock,
  kGlobalAddSystemInfo,
  kGlobalToggleAuth,
  kGlobalAddKioskApp,
  kGlobalRemoveKioskApp,
  kGlobalShowKioskError,
  kGlobalToggleDebugDetachableBase,
  kGlobalCycleDetachableBaseStatus,
  kGlobalCycleDetachableBaseId,
  kGlobalCycleAuthErrorMessage,
  kGlobalToggleWarningBanner,
  kGlobalToggleManagedSessionDisclosure,
  kGlobalShowParentAccess,
  kPerUserTogglePin,
  kPerUserToggleTap,
  kPerUserCycleEasyUnlockState,
  kPerUserCycleFingerprintState,
  kPerUserAuthFingerprintSuccessState,
  kPerUserAuthFingerprintFailState,
  kPerUserForceOnlineSignIn,
  kPerUserToggleAuthEnabled,
  kPerUserUseDetachableBase,
  kPerUserTogglePublicAccount,
};
}  // namespace ButtonId

constexpr const char* kDebugUserNames[] = {
    "Angelina Johnson", "Marcus Cohen", "Chris Wallace",
    "Debbie Craig",     "Stella Wong",  "Stephanie Wade",
};

constexpr const char* kDebugPublicAccountNames[] = {
    "Seattle Public Library", "San Jose Public Library",
    "Sunnyvale Public Library", "Mountain View Public Library",
};

constexpr const char* kDebugDetachableBases[] = {"Base A", "Base B", "Base C"};

constexpr const char kDebugOsVersion[] =
    "Chromium 64.0.3279.0 (Platform 10146.0.0 dev-channel peppy test)";
constexpr const char kDebugEnterpriseInfo[] = "Asset ID: 1111";
constexpr const char kDebugBluetoothName[] = "Bluetooth adapter";

constexpr const char kDebugKioskAppId[] = "asdf1234";
constexpr const char kDebugKioskAppName[] = "Test App Name";

constexpr const char kDebugDefaultLocaleCode[] = "en-GB";
constexpr const char kDebugDefaultLocaleTitle[] = "English";
constexpr const char kDebugEnterpriseDomain[] = "library.com";

// Additional state for a user that the debug UI needs to reference.
struct UserMetadata {
  explicit UserMetadata(const UserInfo& user_info)
      : account_id(user_info.account_id),
        display_name(user_info.display_name),
        type(user_info.type) {}

  AccountId account_id;
  std::string display_name;
  bool enable_pin = false;
  bool enable_tap_to_unlock = false;
  bool enable_auth = true;
  user_manager::UserType type = user_manager::USER_TYPE_REGULAR;
  EasyUnlockIconId easy_unlock_id = EasyUnlockIconId::NONE;
  FingerprintState fingerprint_state = FingerprintState::UNAVAILABLE;
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

  bool is_public_account = type == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
  // Set debug user names and email. Useful for the stub user, which does not
  // have a name  and email set.
  result.basic_user_info.display_name =
      is_public_account
          ? kDebugPublicAccountNames[user_index %
                                     base::size(kDebugPublicAccountNames)]
          : kDebugUserNames[user_index % base::size(kDebugUserNames)];
  result.basic_user_info.display_email =
      result.basic_user_info.account_id.GetUserEmail();

  if (is_public_account) {
    result.public_account_info.emplace();
    result.public_account_info->enterprise_domain = kDebugEnterpriseDomain;
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
      const base::RepeatingClosure& on_users_received)
      : root_dispatcher_(dispatcher),
        lock_screen_note_state_(initial_lock_screen_note_state),
        on_users_received_(on_users_received) {
    root_dispatcher_->AddObserver(this);
  }
  ~DebugDataDispatcherTransformer() override {
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
    if (debug_users_.size() > size_t{count})
      debug_users_.erase(debug_users_.begin() + count, debug_users_.end());

    // Build |users|, add any new users to |debug_users|.
    std::vector<LoginUserInfo> users;
    for (size_t i = 0; i < size_t{count}; ++i) {
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

      if (i >= debug_users_.size())
        debug_users_.push_back(UserMetadata(users[i].basic_user_info));
    }

    return users;
  }

  void NotifyUsers(const std::vector<LoginUserInfo>& users) {
    // User notification resets PIN state.
    for (UserMetadata& user : debug_users_)
      user.enable_pin = false;

    debug_dispatcher_.SetUserList(users);
  }

  int GetUserCount() const { return debug_users_.size(); }

  base::string16 GetDisplayNameForUserIndex(size_t user_index) {
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
    debug_user->enable_pin = !debug_user->enable_pin;
    debug_dispatcher_.SetPinEnabledForUser(debug_user->account_id,
                                           debug_user->enable_pin);
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
  void CycleEasyUnlockForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    // EasyUnlockIconId state transition.
    auto get_next_id = [](EasyUnlockIconId id) {
      switch (id) {
        case EasyUnlockIconId::NONE:
          return EasyUnlockIconId::SPINNER;
        case EasyUnlockIconId::SPINNER:
          return EasyUnlockIconId::LOCKED;
        case EasyUnlockIconId::LOCKED:
          return EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED;
        case EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED:
          return EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT;
        case EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT:
          return EasyUnlockIconId::HARDLOCKED;
        case EasyUnlockIconId::HARDLOCKED:
          return EasyUnlockIconId::UNLOCKED;
        case EasyUnlockIconId::UNLOCKED:
          return EasyUnlockIconId::NONE;
      }
      return EasyUnlockIconId::NONE;
    };
    debug_user->easy_unlock_id = get_next_id(debug_user->easy_unlock_id);

    // Enable/disable click to unlock.
    debug_user->enable_tap_to_unlock =
        debug_user->easy_unlock_id == EasyUnlockIconId::UNLOCKED;

    // Prepare icon that we will show.
    EasyUnlockIconOptions icon;
    icon.icon = debug_user->easy_unlock_id;
    if (icon.icon == EasyUnlockIconId::SPINNER) {
      icon.aria_label = base::ASCIIToUTF16("Icon is spinning");
    } else if (icon.icon == EasyUnlockIconId::LOCKED ||
               icon.icon == EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED) {
      icon.autoshow_tooltip = true;
      icon.tooltip = base::ASCIIToUTF16(
          "This is a long message to trigger overflow. This should show up "
          "automatically. icon_id=" +
          base::NumberToString(static_cast<int>(icon.icon)));
    } else {
      icon.tooltip =
          base::ASCIIToUTF16("This should not show up automatically.");
    }

    // Show icon and enable/disable click to unlock.
    debug_dispatcher_.ShowEasyUnlockIcon(debug_user->account_id, icon);
    debug_dispatcher_.SetTapToUnlockEnabledForUser(
        debug_user->account_id, debug_user->enable_tap_to_unlock);
  }

  // Enables fingerprint auth for the user at |user_index|.
  void CycleFingerprintStateForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    debug_user->fingerprint_state = static_cast<FingerprintState>(
        (static_cast<int>(debug_user->fingerprint_state) + 1) %
        (static_cast<int>(FingerprintState::kMaxValue) + 1));
    debug_dispatcher_.SetFingerprintState(debug_user->account_id,
                                          debug_user->fingerprint_state);
  }
  void AuthenticateFingerprintForUserIndex(size_t user_index, bool success) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_dispatcher_.NotifyFingerprintAuthResult(debug_user->account_id,
                                                  success);
  }

  // Force online sign-in for the user at |user_index|.
  void ForceOnlineSignInForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    debug_dispatcher_.ForceOnlineSignInForUser(
        debug_users_[user_index].account_id);
  }

  // Updates |auth_disabled_reason_| with the next enum value in a cyclic
  // manner.
  void UpdateAuthDisabledReason() {
    switch (auth_disabled_reason_) {
      case AuthDisabledReason::kTimeLimitOverride:
        auth_disabled_reason_ = AuthDisabledReason::kTimeUsageLimit;
        break;
      case AuthDisabledReason::kTimeUsageLimit:
        auth_disabled_reason_ = AuthDisabledReason::kTimeWindowLimit;
        break;
      case AuthDisabledReason::kTimeWindowLimit:
        auth_disabled_reason_ = AuthDisabledReason::kTimeLimitOverride;
        break;
    }
  }

  // Toggle the unlock allowed state for the user at |user_index|.
  void ToggleAuthEnabledForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata& user = debug_users_[user_index];
    user.enable_auth = !user.enable_auth;
    if (user.enable_auth) {
      debug_dispatcher_.EnableAuthForUser(user.account_id);
    } else {
      debug_dispatcher_.DisableAuthForUser(
          user.account_id,
          AuthDisabledData(auth_disabled_reason_,
                           base::Time::Now() +
                               base::TimeDelta::FromHours(user_index) +
                               base::TimeDelta::FromHours(8),
                           base::TimeDelta::FromMinutes(15),
                           true /*bool disable_lock_screen_media*/));
      UpdateAuthDisabledReason();
    }
  }

  // Convert user type to regular user or public account for the user at
  // |user_index|.
  void TogglePublicAccountForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata& user = debug_users_[user_index];
    // Swap the type between regular and public account.
    user.type = user.type == user_manager::USER_TYPE_REGULAR
                    ? user_manager::USER_TYPE_PUBLIC_ACCOUNT
                    : user_manager::USER_TYPE_REGULAR;

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
    KioskAppMenuEntry menu_item;
    menu_item.app_id = kDebugKioskAppId;
    menu_item.name = base::UTF8ToUTF16(kDebugKioskAppName);
    kiosk_apps_.push_back(std::move(menu_item));
    shelf_widget->login_shelf_view()->SetKioskApps(kiosk_apps_, {});
  }

  void RemoveKioskApp(ShelfWidget* shelf_widget) {
    if (kiosk_apps_.empty())
      return;
    kiosk_apps_.pop_back();
    shelf_widget->login_shelf_view()->SetKioskApps(kiosk_apps_, {});
  }

  void AddSystemInfo(const std::string& os_version,
                     const std::string& enterprise_info,
                     const std::string& bluetooth_name,
                     bool adb_sideloading_enabled) {
    debug_dispatcher_.SetSystemInfo(true /*show*/, false /*enforced*/,
                                    os_version, enterprise_info, bluetooth_name,
                                    adb_sideloading_enabled);
  }

  void UpdateWarningMessage(const base::string16& message) {
    debug_dispatcher_.UpdateWarningMessage(message);
  }

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(const std::vector<LoginUserInfo>& users) override {
    // Update root_users_ to new source data.
    root_users_.clear();
    for (auto& user : users)
      root_users_.push_back(user);

    // Rebuild debug users using new source data.
    SetUserCount(root_users_.size());

    on_users_received_.Run();
  }
  void OnPinEnabledForUserChanged(const AccountId& user,
                                  bool enabled) override {
    // Forward notification only if the user is currently being shown.
    for (size_t i = 0u; i < debug_users_.size(); ++i) {
      if (debug_users_[i].account_id == user) {
        debug_users_[i].enable_pin = enabled;
        debug_dispatcher_.SetPinEnabledForUser(user, enabled);
        break;
      }
    }
  }
  void OnTapToUnlockEnabledForUserChanged(const AccountId& user,
                                          bool enabled) override {
    // Forward notification only if the user is currently being shown.
    for (size_t i = 0u; i < debug_users_.size(); ++i) {
      if (debug_users_[i].account_id == user) {
        debug_users_[i].enable_tap_to_unlock = enabled;
        debug_dispatcher_.SetTapToUnlockEnabledForUser(user, enabled);
        break;
      }
    }
  }
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override {
    lock_screen_note_state_ = state;
    debug_dispatcher_.SetLockScreenNoteState(state);
  }
  void OnShowEasyUnlockIcon(const AccountId& user,
                            const EasyUnlockIconOptions& icon) override {
    debug_dispatcher_.ShowEasyUnlockIcon(user, icon);
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

 private:
  // The debug overlay UI takes ground-truth data from |root_dispatcher_|,
  // applies a series of transformations to it, and exposes it to the UI via
  // |debug_dispatcher_|.
  LoginDataDispatcher* root_dispatcher_;  // Unowned.
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

  // When auth is disabled, this property is used to define the reason, which
  // customizes the UI accordingly.
  AuthDisabledReason auth_disabled_reason_ =
      AuthDisabledReason::kTimeLimitOverride;

  DISALLOW_COPY_AND_ASSIGN(DebugDataDispatcherTransformer);
};

// In-memory wrapper around LoginDetachableBaseModel used by lock UI.
// It provides, methods to override the detachable base pairing state seen by
// the UI.
class LockDebugView::DebugLoginDetachableBaseModel
    : public LoginDetachableBaseModel {
 public:
  static constexpr int kNullBaseId = -1;

  DebugLoginDetachableBaseModel() = default;
  ~DebugLoginDetachableBaseModel() override = default;

  bool debugging_pairing_state() const { return pairing_status_.has_value(); }

  // Calculates the pairing status to which the model should be changed when
  // button for cycling detachable base pairing statuses is clicked.
  DetachableBasePairingStatus NextPairingStatus() const {
    if (!pairing_status_.has_value())
      return DetachableBasePairingStatus::kNone;

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
    return (base_id_ + 1) % base::size(kDebugDetachableBases);
  }

  // Gets the descripting text for currently paired base, if any.
  std::string BaseButtonText() const {
    if (base_id_ < 0)
      return "No base";
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
      CHECK_LT(base_id, static_cast<int>(base::size(kDebugDetachableBases)));
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
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated)
      return;
    DCHECK_GE(base_id_, 0);

    last_used_bases_[account_id] = base_id_;
    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(*pairing_status_);
  }

  // Clears all in-memory pairing state.
  void ClearDebugPairingState() {
    pairing_status_ = base::nullopt;
    base_id_ = kNullBaseId;
    last_used_bases_.clear();

    Shell::Get()
        ->login_screen_controller()
        ->data_dispatcher()
        ->SetDetachableBasePairingStatus(DetachableBasePairingStatus::kNone);
  }

  // LoginDetachableBaseModel:
  DetachableBasePairingStatus GetPairingStatus() override {
    if (!pairing_status_.has_value())
      return DetachableBasePairingStatus::kNone;
    return *pairing_status_;
  }
  bool PairedBaseMatchesLastUsedByUser(const UserInfo& user_info) override {
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated)
      return false;

    if (last_used_bases_.count(user_info.account_id) == 0)
      return true;
    return last_used_bases_[user_info.account_id] == base_id_;
  }
  bool SetPairedBaseAsLastUsedByUser(const UserInfo& user_info) override {
    if (GetPairingStatus() != DetachableBasePairingStatus::kAuthenticated)
      return false;

    last_used_bases_[user_info.account_id] = base_id_;
    return true;
  }

 private:
  // In-memory detachable base pairing state.
  base::Optional<DetachableBasePairingStatus> pairing_status_;
  int base_id_ = kNullBaseId;
  // Maps user account to the last used detachable base ID (base ID being the
  // base's index in kDebugDetachableBases array).
  std::map<AccountId, int> last_used_bases_;

  DISALLOW_COPY_AND_ASSIGN(DebugLoginDetachableBaseModel);
};

LockDebugView::LockDebugView(mojom::TrayActionState initial_note_action_state,
                             LockScreen::ScreenType screen_type)
    : debug_data_dispatcher_(std::make_unique<DebugDataDispatcherTransformer>(
          initial_note_action_state,
          Shell::Get()->login_screen_controller()->data_dispatcher(),
          base::BindRepeating(
              &LockDebugView::UpdatePerUserActionContainerAndLayout,
              base::Unretained(this)))),
      next_auth_error_type_(AuthErrorType::kFirstUnlockFailed) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  auto debug_detachable_base_model =
      std::make_unique<DebugLoginDetachableBaseModel>();
  debug_detachable_base_model_ = debug_detachable_base_model.get();

  lock_ = new LockContentsView(initial_note_action_state, screen_type,
                               debug_data_dispatcher_->debug_dispatcher(),
                               std::move(debug_detachable_base_model));
  AddChildView(lock_);

  container_ = new NonAccessibleView();
  container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(container_);

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
  AddButton("Add user", ButtonId::kGlobalAddUser, change_users_container);
  AddButton("Add 10 users", ButtonId::kGlobalAddTenUsers,
            change_users_container);
  AddButton("Remove user", ButtonId::kGlobalRemoveUser, change_users_container);

  auto* toggle_container = add_horizontal_container();
  AddButton("Blur", ButtonId::kGlobalToggleBlur, toggle_container);
  AddButton("Toggle note action", ButtonId::kGlobalToggleNoteAction,
            toggle_container);
  AddButton("Toggle caps lock", ButtonId::kGlobalToggleCapsLock,
            toggle_container);
  AddButton("Add system info", ButtonId::kGlobalAddSystemInfo,
            toggle_container);
  global_action_toggle_auth_ = AddButton(
      "Auth (allowed)", ButtonId::kGlobalToggleAuth, toggle_container);
  AddButton("Cycle auth error", ButtonId::kGlobalCycleAuthErrorMessage,
            toggle_container);
  AddButton("Toggle warning banner", ButtonId::kGlobalToggleWarningBanner,
            toggle_container);
  AddButton("Show parent access", ButtonId::kGlobalShowParentAccess,
            toggle_container);

  auto* kiosk_container = add_horizontal_container();
  AddButton("Add kiosk app", ButtonId::kGlobalAddKioskApp, kiosk_container);
  AddButton("Remove kiosk app", ButtonId::kGlobalRemoveKioskApp,
            kiosk_container);
  AddButton("Show kiosk error", ButtonId::kGlobalShowKioskError,
            kiosk_container);

  auto* managed_sessions_container = add_horizontal_container();
  AddButton("Toggle managed session disclosure",
            ButtonId::kGlobalToggleManagedSessionDisclosure,
            managed_sessions_container);

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
    scroll->SetBackgroundColor(SK_ColorTRANSPARENT);
    scroll->SetVerticalScrollBar(
        std::make_unique<views::OverlayScrollBar>(false));
    scroll->SetHorizontalScrollBar(
        std::make_unique<views::OverlayScrollBar>(true));
    return scroll;
  };
  container_->AddChildView(make_scroll(global_action_view_container_, 110));
  container_->AddChildView(make_scroll(per_user_action_view_container_, 100));

  Layout();
}

LockDebugView::~LockDebugView() {
  // Make sure debug_data_dispatcher_ lives longer than LockContentsView so
  // pointer debug_dispatcher_ is always valid for LockContentsView.
  RemoveChildView(lock_);
}

void LockDebugView::Layout() {
  global_action_view_container_->SizeToPreferredSize();
  per_user_action_view_container_->SizeToPreferredSize();

  views::View::Layout();

  lock_->SetBoundsRect(GetLocalBounds());
  container_->SetPosition(gfx::Point());
  container_->SizeToPreferredSize();
}

void LockDebugView::CycleAuthErrorMessage() {
  switch (next_auth_error_type_) {
    case AuthErrorType::kFirstUnlockFailed:
      next_auth_error_type_ = AuthErrorType::kFirstUnlockFailedCapsLockOn;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          false /*caps_enabled*/);
      debug_detachable_base_model_->SetPairingState(
          DetachableBasePairingStatus::kNone,
          DebugLoginDetachableBaseModel::kNullBaseId);
      lock_->ShowAuthErrorMessageForDebug(1 /*unlock_attempt*/);
      return;
    case AuthErrorType::kFirstUnlockFailedCapsLockOn:
      next_auth_error_type_ = AuthErrorType::kSecondUnlockFailed;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          true /*caps_enabled*/);
      lock_->ShowAuthErrorMessageForDebug(1 /*unlock_attempt*/);
      return;
    case AuthErrorType::kSecondUnlockFailed:
      next_auth_error_type_ = AuthErrorType::kSecondUnlockFailedCapsLockOn;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          false /*caps_enabled*/);
      lock_->ShowAuthErrorMessageForDebug(2 /*unlock_attempt*/);
      return;
    case AuthErrorType::kSecondUnlockFailedCapsLockOn:
      next_auth_error_type_ = AuthErrorType::kDetachableBaseFailed;
      Shell::Get()->ime_controller()->UpdateCapsLockState(
          true /*caps_enabled*/);
      lock_->ShowAuthErrorMessageForDebug(2 /*unlock_attempt*/);
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

void LockDebugView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  // Add or remove a user.
  bool is_add_user = sender->GetID() == ButtonId::kGlobalAddUser;
  bool is_add_many_users = sender->GetID() == ButtonId::kGlobalAddTenUsers;
  bool is_remove_user = sender->GetID() == ButtonId::kGlobalRemoveUser;
  if (is_add_user || is_add_many_users || is_remove_user) {
    int num_users = debug_data_dispatcher_->GetUserCount();
    if (is_add_user)
      ++num_users;
    else if (is_add_many_users)
      num_users += 10;
    else if (is_remove_user)
      --num_users;
    if (num_users < 0)
      num_users = 0;
    debug_data_dispatcher_->SetUserCount(num_users);
    UpdatePerUserActionContainer();
    Layout();
    return;
  }

  // Enable or disable wallpaper blur.
  if (sender->GetID() == ButtonId::kGlobalToggleBlur) {
    Shell::Get()->wallpaper_controller()->UpdateWallpaperBlur(
        !Shell::Get()->wallpaper_controller()->IsWallpaperBlurred());
    return;
  }

  // Enable or disable note action.
  if (sender->GetID() == ButtonId::kGlobalToggleNoteAction) {
    debug_data_dispatcher_->ToggleLockScreenNoteButton();
    return;
  }

  // Enable or disable caps lock.
  if (sender->GetID() == ButtonId::kGlobalToggleCapsLock) {
    ImeController* ime_controller = Shell::Get()->ime_controller();
    ime_controller->SetCapsLockEnabled(!ime_controller->IsCapsLockEnabled());
    return;
  }

  // Iteratively adds more info to the system info labels to test 7 permutations
  // and then disables the button.
  if (sender->GetID() == ButtonId::kGlobalAddSystemInfo) {
    DCHECK_LT(num_system_info_clicks_, 7u);
    ++num_system_info_clicks_;
    if (num_system_info_clicks_ == 7u)
      sender->SetEnabled(false);

    std::string os_version = num_system_info_clicks_ / 4 ? kDebugOsVersion : "";
    std::string enterprise_info =
        (num_system_info_clicks_ % 4) / 2 ? kDebugEnterpriseInfo : "";
    std::string bluetooth_name =
        num_system_info_clicks_ % 2 ? kDebugBluetoothName : "";
    bool adb_sideloading_enabled = num_system_info_clicks_ % 3;
    debug_data_dispatcher_->AddSystemInfo(
        os_version, enterprise_info, bluetooth_name, adb_sideloading_enabled);
    return;
  }

  // Enable/disable auth. This is useful for testing auth failure scenarios on
  // Linux Desktop builds, where the cryptohome dbus stub accepts all passwords
  // as valid.
  if (sender->GetID() == ButtonId::kGlobalToggleAuth) {
    auto get_next_auth_state = [](LoginScreenController::ForceFailAuth auth) {
      switch (auth) {
        case LoginScreenController::ForceFailAuth::kOff:
          return LoginScreenController::ForceFailAuth::kImmediate;
        case LoginScreenController::ForceFailAuth::kImmediate:
          return LoginScreenController::ForceFailAuth::kDelayed;
        case LoginScreenController::ForceFailAuth::kDelayed:
          return LoginScreenController::ForceFailAuth::kOff;
      }
      NOTREACHED();
      return LoginScreenController::ForceFailAuth::kOff;
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
      NOTREACHED();
      return "Auth (allowed)";
    };
    force_fail_auth_ = get_next_auth_state(force_fail_auth_);
    global_action_toggle_auth_->SetText(
        base::ASCIIToUTF16(get_auth_label(force_fail_auth_)));
    Layout();
    Shell::Get()
        ->login_screen_controller()
        ->set_force_fail_auth_for_debug_overlay(force_fail_auth_);
    return;
  }

  if (sender->GetID() == ButtonId::kGlobalAddKioskApp) {
    debug_data_dispatcher_->AddKioskApp(
        Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
  }

  if (sender->GetID() == ButtonId::kGlobalRemoveKioskApp) {
    debug_data_dispatcher_->RemoveKioskApp(
        Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
  }

  if (sender->GetID() == ButtonId::kGlobalShowKioskError) {
    Shell::Get()->login_screen_controller()->ShowKioskAppError(
        "Test error message.");
  }

  if (sender->GetID() == ButtonId::kGlobalToggleDebugDetachableBase) {
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
    Layout();
    return;
  }

  if (sender->GetID() == ButtonId::kGlobalCycleDetachableBaseStatus) {
    debug_detachable_base_model_->SetPairingState(
        debug_detachable_base_model_->NextPairingStatus(),
        debug_detachable_base_model_->NextBaseId());
    UpdatePerUserActionContainer();
    UpdateDetachableBaseColumn();
    Layout();
    return;
  }

  if (sender->GetID() == ButtonId::kGlobalCycleDetachableBaseId) {
    debug_detachable_base_model_->SetPairingState(
        DetachableBasePairingStatus::kAuthenticated,
        debug_detachable_base_model_->NextBaseId());
    UpdateDetachableBaseColumn();
    Layout();
    return;
  }

  if (sender->GetID() == ButtonId::kGlobalCycleAuthErrorMessage) {
    CycleAuthErrorMessage();
    return;
  }

  // Show or hide warning banner.
  if (sender->GetID() == ButtonId::kGlobalToggleWarningBanner) {
    if (is_warning_banner_shown_) {
      debug_data_dispatcher_->UpdateWarningMessage({});
    } else {
      debug_data_dispatcher_->UpdateWarningMessage(base::ASCIIToUTF16(
          "A critical update is ready to install. Sign in to get started."));
    }
    is_warning_banner_shown_ = !is_warning_banner_shown_;
  }

  // Enable or disable PIN.
  if (sender->GetID() == ButtonId::kPerUserTogglePin)
    debug_data_dispatcher_->TogglePinStateForUserIndex(sender->tag());

  // Enable or disable tap.
  if (sender->GetID() == ButtonId::kPerUserToggleTap)
    debug_data_dispatcher_->ToggleTapStateForUserIndex(sender->tag());

  // Cycle easy unlock.
  if (sender->GetID() == ButtonId::kPerUserCycleEasyUnlockState)
    debug_data_dispatcher_->CycleEasyUnlockForUserIndex(sender->tag());

  // Cycle fingerprint unlock state.
  if (sender->GetID() == ButtonId::kPerUserCycleFingerprintState)
    debug_data_dispatcher_->CycleFingerprintStateForUserIndex(sender->tag());
  if (sender->GetID() == ButtonId::kPerUserAuthFingerprintSuccessState) {
    debug_data_dispatcher_->AuthenticateFingerprintForUserIndex(sender->tag(),
                                                                true);
  }
  if (sender->GetID() == ButtonId::kPerUserAuthFingerprintFailState) {
    debug_data_dispatcher_->AuthenticateFingerprintForUserIndex(sender->tag(),
                                                                false);
  }

  if (sender->GetID() == ButtonId::kGlobalToggleManagedSessionDisclosure) {
    is_managed_session_disclosure_shown_ =
        !is_managed_session_disclosure_shown_;
    debug_data_dispatcher_->OnPublicSessionShowFullManagementDisclosureChanged(
        is_managed_session_disclosure_shown_);
  }

  // Force online sign-in.
  if (sender->GetID() == ButtonId::kPerUserForceOnlineSignIn)
    debug_data_dispatcher_->ForceOnlineSignInForUserIndex(sender->tag());

  // Enable or disable auth.
  if (sender->GetID() == ButtonId::kPerUserToggleAuthEnabled)
    debug_data_dispatcher_->ToggleAuthEnabledForUserIndex(sender->tag());

  // Update the last used detachable base.
  if (sender->GetID() == ButtonId::kPerUserUseDetachableBase) {
    debug_detachable_base_model_->SetBaseLastUsedForUser(
        debug_data_dispatcher_->GetAccountIdForUserIndex(sender->tag()));
  }

  // Convert this user to regular user or public account.
  if (sender->GetID() == ButtonId::kPerUserTogglePublicAccount) {
    debug_data_dispatcher_->TogglePublicAccountForUserIndex(sender->tag());
    UpdatePerUserActionContainer();
    Layout();
  }

  // Show parent access dialog. It is modal dialog that blocks access to
  // underlying UI. It is not possible to create another instance before the
  // existing one is dismissed in dev overlay.
  if (sender->GetID() == ButtonId::kGlobalShowParentAccess)
    lock_->ShowParentAccessDialog();
}

void LockDebugView::UpdatePerUserActionContainer() {
  per_user_action_view_container_->RemoveAllChildViews(
      true /*delete_children*/);

  int num_users = debug_data_dispatcher_->GetUserCount();
  for (int i = 0; i < num_users; ++i) {
    auto* row = new NonAccessibleView();
    row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    auto* name = new views::Label();
    name->SetText(debug_data_dispatcher_->GetDisplayNameForUserIndex(i));
    name->SetSubpixelRenderingEnabled(false);
    name->SetEnabledColor(SK_ColorWHITE);
    name->SetAutoColorReadabilityEnabled(false);
    // name->SetFontList(name->font_list().DeriveWithSizeDelta(3));
    row->AddChildView(name);

    AddButton("Toggle PIN", ButtonId::kPerUserTogglePin, row)->set_tag(i);
    AddButton("Toggle Tap", ButtonId::kPerUserToggleTap, row)->set_tag(i);
    AddButton("Cycle easy unlock", ButtonId::kPerUserCycleEasyUnlockState, row)
        ->set_tag(i);
    AddButton("Cycle fingerprint state",
              ButtonId::kPerUserCycleFingerprintState, row)
        ->set_tag(i);
    AddButton("Send fingerprint auth success",
              ButtonId::kPerUserAuthFingerprintSuccessState, row)
        ->set_tag(i);
    AddButton("Send fingerprint auth fail",
              ButtonId::kPerUserAuthFingerprintFailState, row)
        ->set_tag(i);
    AddButton("Force online sign-in", ButtonId::kPerUserForceOnlineSignIn, row)
        ->set_tag(i);
    AddButton("Toggle auth enabled", ButtonId::kPerUserToggleAuthEnabled, row)
        ->set_tag(i);

    if (debug_detachable_base_model_->debugging_pairing_state() &&
        debug_detachable_base_model_->GetPairingStatus() ==
            DetachableBasePairingStatus::kAuthenticated) {
      AddButton("Set base used", ButtonId::kPerUserUseDetachableBase, row)
          ->set_tag(i);
    }

    AddButton("Toggle Public Account", ButtonId::kPerUserTogglePublicAccount,
              row)
        ->set_tag(i);

    per_user_action_view_container_->AddChildView(row);
  }
}

void LockDebugView::UpdatePerUserActionContainerAndLayout() {
  UpdatePerUserActionContainer();
  Layout();
}

void LockDebugView::UpdateDetachableBaseColumn() {
  global_action_detachable_base_group_->RemoveAllChildViews(
      true /*delete_children*/);

  AddButton("Debug detachable base", ButtonId::kGlobalToggleDebugDetachableBase,
            global_action_detachable_base_group_);
  if (!debug_detachable_base_model_->debugging_pairing_state())
    return;

  const std::string kPairingStatusText =
      "Pairing status: " +
      DetachableBasePairingStatusToString(
          debug_detachable_base_model_->GetPairingStatus());
  AddButton(kPairingStatusText, ButtonId::kGlobalCycleDetachableBaseStatus,
            global_action_detachable_base_group_);

  views::LabelButton* cycle_detachable_base_id =
      AddButton(debug_detachable_base_model_->BaseButtonText(),
                ButtonId::kGlobalCycleDetachableBaseId,
                global_action_detachable_base_group_);
  bool base_authenticated = debug_detachable_base_model_->GetPairingStatus() ==
                            DetachableBasePairingStatus::kAuthenticated;
  cycle_detachable_base_id->SetEnabled(base_authenticated);
}

views::LabelButton* LockDebugView::AddButton(const std::string& text,
                                             int id,
                                             views::View* container) {
  // Creates a button with |text| that cannot be focused.
  std::unique_ptr<views::LabelButton> button =
      views::MdTextButton::CreateSecondaryUiButton(this,
                                                   base::ASCIIToUTF16(text));
  button->SetID(id);
  button->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  views::LabelButton* view = button.get();
  container->AddChildView(
      login_views_utils::WrapViewForPreferredSize(std::move(button)));
  return view;
}

}  // namespace ash
