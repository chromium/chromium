// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

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
class UserManager;
}  // namespace user_manager

namespace ash {

namespace system {
class SystemClock;
}  // namespace system

class ScreenLocker;
class SessionManagerClient;
class SessionTerminationManager;
class UserAddingScreen;

class ScreenLockerController : public UserAddingScreen::Observer,
                               public session_manager::SessionManagerObserver {
 public:
  // Returns a pointer to the singleton instance.
  static ScreenLockerController& Get();

  // `local_state`, `application_locale_storage`,
  // `browser_policy_connector_ash`, `session_manager_client`,
  // `session_termination_manager`, `session_manager`, `user_manager`,
  // `user_adding_screen`, `multi_user_sign_in_policy_controller`, and
  // `system_clock` must be non-null and must outlive `this`.
  // `shared_url_loader_factory` must be non-null.
  ScreenLockerController(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      SessionManagerClient* session_manager_client,
      SessionTerminationManager* session_termination_manager,
      session_manager::SessionManager* session_manager,
      user_manager::UserManager* user_manager,
      UserAddingScreen* user_adding_screen,
      const user_manager::MultiUserSignInPolicyController*
          multi_user_sign_in_policy_controller,
      system::SystemClock* system_clock);

  ScreenLockerController(const ScreenLockerController&) = delete;
  ScreenLockerController& operator=(const ScreenLockerController&) = delete;
  ~ScreenLockerController() override;

  ScreenLocker* screen_locker() const { return screen_locker_.get(); }

  // Handles a request from the session manager to show the lock screen.
  void HandleShowLockScreenRequest();

  // Shows the lock screen.
  void ShowLockScreen();

  // Hides the lock screen.
  void HideLockScreen();

 private:
  void CreateAndInitScreenLocker();
  void DestroyScreenLocker();
  void OnUnlockAnimationFinished(bool aborted);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // UserAddingScreen::Observer:
  void OnUserAddingFinished() override;

  const raw_ref<PrefService> local_state_;
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
  const raw_ref<policy::BrowserPolicyConnectorAsh>
      browser_policy_connector_ash_;

  const raw_ref<SessionManagerClient> session_manager_client_;
  const raw_ref<SessionTerminationManager> session_termination_manager_;
  const raw_ref<session_manager::SessionManager> session_manager_;
  const raw_ref<user_manager::UserManager> user_manager_;
  const raw_ref<UserAddingScreen> user_adding_screen_;
  const raw_ref<const user_manager::MultiUserSignInPolicyController>
      multi_user_sign_in_policy_controller_;
  const raw_ref<system::SystemClock> system_clock_;

  std::unique_ptr<ScreenLocker> screen_locker_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::ScopedObservation<UserAddingScreen, UserAddingScreen::Observer>
      user_adding_screen_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ScreenLockerController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_CONTROLLER_H_
