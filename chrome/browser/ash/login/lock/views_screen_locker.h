// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/lock_screen_apps/focus_cycler_delegate.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"

namespace ash {

class MojoSystemInfoDispatcher;
class UserSelectionScreen;

// ViewsScreenLocker acts like LoginScreenClientImpl::Delegate which handles
// method calls coming from ash into chrome.
// It also handles calls from chrome into ash (views-based lockscreen).
class ViewsScreenLocker : public LoginScreenClientImpl::Delegate,
                          public chromeos::PowerManagerClient::Observer,
                          public lock_screen_apps::FocusCyclerDelegate {
 public:
  ViewsScreenLocker();

  ViewsScreenLocker(const ViewsScreenLocker&) = delete;
  ViewsScreenLocker& operator=(const ViewsScreenLocker&) = delete;

  ~ViewsScreenLocker() override;

  void Init(const user_manager::UserList& users);

  // Called by ScreenLocker to notify that ash lock animation finishes.
  void OnAshLockAnimationFinished();

  // LoginScreenClientImpl::Delegate
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleAuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // lock_screen_apps::FocusCyclerDelegate:
  void RegisterLockScreenAppFocusHandler(
      const LockScreenAppFocusCallback& focus_handler) override;
  void UnregisterLockScreenAppFocusHandler() override;
  void HandleLockScreenAppFocusOut(bool reverse) override;

 private:
  void OnAuthenticated(const AccountId& account_id,
                       base::OnceCallback<void(bool)> success_callback,
                       bool success);
  void UpdateAuthFactorsAvailability(const user_manager::User* user);
  void UpdatePinKeyboardState(const AccountId& account_id);
  void UpdateChallengeResponseAuthAvailability(const AccountId& account_id);
  void OnAuthSessionStarted(bool user_exists,
                            std::unique_ptr<ash::UserContext> user_context,
                            std::optional<ash::AuthenticationError> error);
  void OnPinCanAuthenticate(const AccountId& account_id,
                            bool can_authenticate,
                            cryptohome::PinLockAvailability available_at);

  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  // Callback registered as a lock screen apps focus handler - it should be
  // called to hand focus over to lock screen apps.
  LockScreenAppFocusCallback lock_screen_app_focus_handler_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  AuthPerformer auth_performer_;

  base::WeakPtrFactory<ViewsScreenLocker> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
