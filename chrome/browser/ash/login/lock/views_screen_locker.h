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
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"

class ApplicationLocaleStorage;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace user_manager {
class MultiUserSignInPolicyController;
}  // namespace user_manager

namespace ash {

namespace system {
class SystemClock;
}  // namespace system

class MojoSystemInfoDispatcher;
class UserSelectionScreen;

// ViewsScreenLocker acts like LoginScreenClientImpl::Delegate which handles
// method calls coming from ash into chrome.
// It also handles calls from chrome into ash (views-based lockscreen).
class ViewsScreenLocker : public LoginScreenClientImpl::Delegate,
                          public chromeos::PowerManagerClient::Observer {
 public:
  // `local_state`, `application_locale_storage`,
  // `browser_policy_connector_ash`, `multi_user_sign_in_policy_controller`,
  // and `system_clock` must be non-null and must outlive `this`.
  // `shared_url_loader_factory` must be non-null.
  ViewsScreenLocker(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      const user_manager::MultiUserSignInPolicyController*
          multi_user_sign_in_policy_controller,
      system::SystemClock* system_clock);

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
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;

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

  const raw_ref<PrefService> local_state_;

  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  AuthPerformer auth_performer_;

  base::WeakPtrFactory<ViewsScreenLocker> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
