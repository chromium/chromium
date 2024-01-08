// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/login/user_online_signin_notifier.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"

class AccountId;

namespace ash {

class SmartLockService;
class UserBoardView;
struct LoginUserInfo;

enum class DisplayedScreen { SIGN_IN_SCREEN, USER_ADDING_SCREEN, LOCK_SCREEN };

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen
    : public proximity_auth::ScreenlockBridge::LockHandler,
      public session_manager::SessionManagerObserver,
      public PasswordSyncTokenLoginChecker::Observer,
      public UserOnlineSigninNotifier::Observer {
 public:
  explicit UserSelectionScreen(DisplayedScreen display_type);

  UserSelectionScreen(const UserSelectionScreen&) = delete;
  UserSelectionScreen& operator=(const UserSelectionScreen&) = delete;

  ~UserSelectionScreen() override;

  void SetView(UserBoardView* view);

  static const user_manager::UserList PrepareUserListForSending(
      const user_manager::UserList& users,
      const AccountId& owner,
      bool is_signin_to_add);

  virtual void Init(const user_manager::UserList& users);

  void CheckUserStatus(const AccountId& account_id);
  void HandleFocusPod(const AccountId& account_id);
  void HandleNoPodFocused();
  void OnBeforeShow();

  void AttemptEasyUnlock(const AccountId& account_id);
  void InitEasyUnlock();

  void SetTpmLockedState(bool is_locked, base::TimeDelta time_left);

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override;
  void SetSmartLockState(const AccountId& account_id,
                         SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool success) override;

  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const std::u16string& auth_value) override;
  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;

  void Unlock(const AccountId& account_id) override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  // PasswordSyncTokenLoginChecker::Observer
  void OnInvalidSyncToken(const AccountId& account_id) override;

  // UserOnlineSigninNotifier::Observer
  void OnOnlineSigninEnforced(const AccountId& account_id) override;

  std::vector<LoginUserInfo> UpdateAndReturnUserListForAsh();
  void SetUsersLoaded(bool loaded);

 protected:
  raw_ptr<UserBoardView> view_ = nullptr;

  // Map from public session account IDs to recommended locales set by policy.
  std::map<AccountId, std::vector<std::string>>
      public_session_recommended_locales_;

  // Whether users have been sent to the UI(WebUI or Views).
  bool users_loaded_ = false;

 private:
  class DircryptoMigrationChecker;
  class TpmLockedChecker;

  SmartLockService* GetSmartLockServiceForUser(
      const AccountId& account_id) const;

  void OnUserStatusChecked(const AccountId& account_id,
                           const std::string& token,
                           bool reauth_required);
  void OnAllowedInputMethodsChanged();

  // Purpose of the screen.
  const DisplayedScreen display_type_;

  // Set of Users that are visible.
  user_manager::UserList users_;

  // Map of account ids to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<AccountId, proximity_auth::mojom::AuthType> user_auth_type_map_;

  // Token handler util for checking user OAuth token status.
  std::unique_ptr<TokenHandleUtil> token_handle_util_;

  // Helper to check whether a user needs dircrypto migration.
  std::unique_ptr<DircryptoMigrationChecker> dircrypto_migration_checker_;

  // Helper to check whether TPM is locked or not.
  std::unique_ptr<TpmLockedChecker> tpm_locked_checker_;

  user_manager::UserList users_to_send_;

  AccountId focused_pod_account_id_;
  std::optional<system::SystemClock::ScopedHourClockType>
      focused_user_clock_type_;

  // Sometimes we might get focused pod while user session is still active. e.g.
  // while creating lock screen. So postpone any work until after the session
  // state changes.
  std::optional<AccountId> pending_focused_account_id_;

  // Input Method Engine state used at the user selection screen.
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  base::CallbackListSubscription allowed_input_methods_subscription_;

  // Collection of verifiers that check validity of password sync token for SAML
  // users corresponding to visible pods.
  std::unique_ptr<PasswordSyncTokenCheckersCollection> sync_token_checkers_;

  // Notifies on enforced online signin per user.
  std::unique_ptr<UserOnlineSigninNotifier> online_signin_notifier_;

  base::ScopedObservation<UserOnlineSigninNotifier,
                          UserOnlineSigninNotifier::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<UserSelectionScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
