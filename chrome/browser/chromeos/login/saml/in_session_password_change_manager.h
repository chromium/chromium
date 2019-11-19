// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/session/session_activation_observer.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/login/auth/auth_status_consumer.h"

class Profile;

namespace user_manager {
class User;
}

namespace chromeos {
class CryptohomeAuthenticator;
class UserContext;

// There is at most one instance of this task, which is part of the
// InSessionPasswordChangeManager singleton. Having a separate class means that
// pointers to this class can be invalidated without affecting the manager.
// Calling Recheck or RecheckAfter invalidates the existing pointers before
// rerunning the task or posting the task to be re-run, which means that there
// is only ever one of task that is scheduled to be run.
class RecheckPasswordExpiryTask {
 private:
  RecheckPasswordExpiryTask();
  ~RecheckPasswordExpiryTask();

  // Delegates to InSessionPasswordChangeManager::MaybeShowExpiryNotification.
  void Recheck();

  // Calls Recheck after the given |delay|.
  void RecheckAfter(base::TimeDelta delay);

  // Cancels any pending calls to Recheck that are scheduled..
  void CancelPendingRecheck();

  base::WeakPtrFactory<RecheckPasswordExpiryTask> weak_ptr_factory_{this};

  // Only InSessionPasswordChangeManager can use this class.
  friend class InSessionPasswordChangeManager;

  DISALLOW_COPY_AND_ASSIGN(RecheckPasswordExpiryTask);
};

// Manages the flow of changing a password in-session - handles user
// response from dialogs, and callbacks from subsystems.
// This singleton is scoped to the primary user session - it will exist for as
// long as the primary user session exists  (but only if the primary user's
// InSessionPasswordChange policy is enabled and the kInSessionPasswordChange
// feature is enabled).
class InSessionPasswordChangeManager : public AuthStatusConsumer,
                                       public ash::SessionActivationObserver {
 public:
  // Events in the in-session SAML password change flow.
  enum class Event {
    // Dialog is open showing third-party IdP SAML password change page:
    START_SAML_IDP_PASSWORD_CHANGE,
    // Third party IdP SAML password is changed (but not cryptohome yet):
    SAML_IDP_PASSWORD_CHANGED,
    // Async call to change cryptohome password is started:
    START_CRYPTOHOME_PASSWORD_CHANGE,
    // Change of cryptohome password failed - wrong old password:
    CRYPTOHOME_PASSWORD_CHANGE_FAILURE,
    // Change of cryptohome password complete. In session PW change complete.
    CRYPTOHOME_PASSWORD_CHANGED,
  };

  // How the passwords were able to be obtained.
  enum class PasswordSource {
    PASSWORDS_SCRAPED,  // Passwords were scraped during SAML password change.
    PASSWORDS_RETYPED,  // Passwords had to be manually confirmed by user.
  };

  // Observers of InSessionPasswordChangeManager are notified of certain events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEvent(Event event) = 0;
  };

  // Returns null if in-session password change is disabled.
  static std::unique_ptr<InSessionPasswordChangeManager> CreateIfEnabled(
      Profile* primary_profile);

  // Returns true if the InSessionPasswordChangeManager is both enabled and
  // ready, so Get() can safely be called.
  static bool IsInitialized();

  // Checks that the InSessionPasswordChangeManager is both enabled and ready,
  // then returns it.
  static InSessionPasswordChangeManager* Get();

  explicit InSessionPasswordChangeManager(Profile* primary_profile);
  ~InSessionPasswordChangeManager() override;

  // Sets the given instance as the singleton for testing.
  static void SetForTesting(InSessionPasswordChangeManager* instance);
  static void ResetForTesting();

  // Checks if the primary user's password has expired or will soon expire, and
  // shows a notification if needed. If the password will expire in the distant
  // future, posts a task to check again in the distant future.
  void MaybeShowExpiryNotification();

  // Shows a password expiry notification. If |time_until_expiry| is zero or
  // negative then the password has already expired.
  void ShowStandardExpiryNotification(base::TimeDelta time_until_expiry);

  // Shows an urgent password expiry notification. This notification displays
  // a live countdown until password expiry.
  void ShowUrgentExpiryNotification();

  // Dismiss password expiry notification and dismiss urgent password expiry
  // notification, if either are shown.
  void DismissExpiryNotification();

  // User dismissed a notification - make sure not to show it again immediately,
  // even if the password is still scheduled to expire soon.
  void OnExpiryNotificationDismissedByUser();

  // When the screen is unlocked, password expiry notifications are reshown (if
  // they are not already dismissed). On each unlock, the notification pops
  // out of the system tray and is visible on screen again for a few seconds.
  void OnScreenUnlocked();

  // Start the in-session password change flow by showing a dialog that embeds
  // the user's SAML IdP change-password page:
  void StartInSessionPasswordChange();

  // Handle a SAML password change. |old_password| and |new_password| can be
  // empty if scraping failed, in which case the user will be prompted to enter
  // them again. If they are scraped, this calls ChangePassword immediately,
  void OnSamlPasswordChanged(const std::string& scraped_old_password,
                             const std::string& scraped_new_password);

  // Change cryptohome password for primary user.
  void ChangePassword(const std::string& old_password,
                      const std::string& new_password,
                      PasswordSource password_source);

  // Handle a failure to scrape the passwords during in-session password change,
  // by showing a dialog for the user to confirm their old + new password.
  void HandlePasswordScrapeFailure();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnPasswordChangeDetected() override;
  void OnAuthSuccess(const UserContext& user_context) override;

  // ash::SessionActivationObserver:
  void OnSessionActivated(bool activated) override;
  void OnLockStateChanged(bool locked) override;

 private:
  static InSessionPasswordChangeManager* GetNullable();

  void NotifyObservers(Event event);

  Profile* primary_profile_;
  const user_manager::User* primary_user_;
  base::ObserverList<Observer> observer_list_;
  RecheckPasswordExpiryTask recheck_task_;
  scoped_refptr<CryptohomeAuthenticator> authenticator_;
  int urgent_warning_days_;
  bool renotify_on_unlock_ = false;
  PasswordSource password_source_ = PasswordSource::PASSWORDS_SCRAPED;

  friend class InSessionPasswordChangeManagerTest;

  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
