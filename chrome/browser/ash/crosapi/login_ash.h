// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the Login crosapi interface.
class LoginAsh : public mojom::Login {
 public:
  class ExternalLogoutDoneObserver : public base::CheckedObserver {
   public:
    virtual void OnExternalLogoutDone() {}
  };

  LoginAsh();
  LoginAsh(const LoginAsh&) = delete;
  LoginAsh& operator=(const LoginAsh&) = delete;
  ~LoginAsh() override;

  using OptionalErrorCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;

  void BindReceiver(mojo::PendingReceiver<mojom::Login> receiver);

  // crosapi::mojom::Login:
  void ExitCurrentSession(
      const std::optional<std::string>& data_for_next_login_attempt,
      ExitCurrentSessionCallback callback) override;
  void FetchDataForNextLoginAttempt(
      FetchDataForNextLoginAttemptCallback callback) override;
  void LockManagedGuestSession(
      LockManagedGuestSessionCallback callback) override;
  void LockCurrentSession(LockCurrentSessionCallback callback) override;
  void EndSharedSession(EndSharedSessionCallback callback) override;
  void SetDataForNextLoginAttempt(
      const std::string& data_for_next_login_attempt,
      SetDataForNextLoginAttemptCallback callback) override;
  void AddLacrosCleanupTriggeredObserver(
      mojo::PendingRemote<mojom::LacrosCleanupTriggeredObserver> observer)
      override;
  void AddExternalLogoutRequestObserver(
      mojo::PendingRemote<mojom::ExternalLogoutRequestObserver> observer)
      override;
  void NotifyOnExternalLogoutDone() override;
  void ShowGuestSessionConfirmationDialog() override;
  // Methods that are removed from mojom::Login interface. The methods cannot be
  // completely removed, only renamed, because the interface is Stable and has
  // to preserve backward-compatibility.
  void REMOVED_0(const std::optional<std::string>& password,
                 REMOVED_0Callback callback) override;
  void REMOVED_4(const std::string& password,
                 REMOVED_4Callback callback) override;
  void REMOVED_5(const std::string& password,
                 REMOVED_5Callback callback) override;
  void REMOVED_6(const std::string& password,
                 REMOVED_6Callback callback) override;
  void REMOVED_7(const std::string& password,
                 REMOVED_7Callback callback) override;
  void REMOVED_10(mojom::SamlUserSessionPropertiesPtr properties,
                  REMOVED_10Callback callback) override;
  void REMOVED_12(const std::string& password,
                  REMOVED_12Callback callback) override;

  // Launches a managed guest session if one is set up via the admin console.
  // If there are several managed guest sessions set up, it will launch the
  // first available one.
  // If a password is provided, the Managed Guest Session will be lockable and
  // can be unlocked by providing the same password to
  // `UnlockManagedGuestSession()`.
  void LaunchManagedGuestSession(const std::optional<std::string>& password,
                                 OptionalErrorCallback callback);
  // Deprecated. Use `UnlockCurrentSession()` below.
  void UnlockManagedGuestSession(const std::string& password,
                                 OptionalErrorCallback callback);

  // Starts a ChromeOS Managed Guest Session which will host the shared user
  // sessions. An initial shared session is entered with `password` as the
  // password. When this shared session is locked, it can only be unlocked by
  // calling `UnlockSharedSession()` with the same password.
  void LaunchSharedManagedGuestSession(const std::string& password,
                                       OptionalErrorCallback callback);
  // Enters the shared session with the given password. If the session is
  // locked, it can only be unlocked by calling `UnlockSharedSession()` with
  // the same password.
  // Fails if  there is already a shared session running. Can only be called
  // from the lock screen.
  void EnterSharedSession(const std::string& password,
                          OptionalErrorCallback callback);
  // Unlocks the shared session with the provided password. Fails if the
  // password does not match the one provided to
  // `LaunchSharedManagedGuestSession()` or `EnterSharedSession()`.
  // Fails if  there is no existing shared session. Can only be called from the
  // lock screen.
  void UnlockSharedSession(const std::string& password,
                           OptionalErrorCallback callback);

  // Launches a SAML user session with the provided email, gaiaId, password
  // and oauth_code cookie.
  void LaunchSamlUserSession(const std::string& email,
                             const std::string& gaia_id,
                             const std::string& password,
                             const std::string& oauth_code,
                             OptionalErrorCallback callback);
  // Unlocks the current session. The session has to be either a user session or
  // a Managed Guest Session launched by `LaunchManagedGuestSession()` with a
  // password. The session will unlock if `password` matches the one provided
  // to at launch.
  void UnlockCurrentSession(const std::string& password,
                            OptionalErrorCallback callback);

  // Adds an observer for the external logout done events.
  void AddExternalLogoutDoneObserver(ExternalLogoutDoneObserver* observer);
  // Required for the below `base::ObserverList`:
  void RemoveExternalLogoutDoneObserver(ExternalLogoutDoneObserver* observer);
  // Notifies the external logout observers with the
  // `login.onRequestExternalLogout` event. It is called from the login screen
  // extension running on the lock screen (ash-chrome). The in-session extension
  // (lacros/ash-chrome) listens for the dispatched event.
  void NotifyOnRequestExternalLogout();

  mojo::RemoteSet<mojom::LacrosCleanupTriggeredObserver>&
  GetCleanupTriggeredObservers();

 private:
  void OnScreenLockerAuthenticate(OptionalErrorCallback callback, bool success);
  void OnOptionalErrorCallbackComplete(OptionalErrorCallback callback,
                                       const std::optional<std::string>& error);
  std::optional<std::string> CanLaunchSession();
  std::optional<std::string> LockSession(
      std::optional<user_manager::UserType> user_type = std::nullopt);
  std::optional<std::string> CanUnlockSession(
      std::optional<user_manager::UserType> user_type = std::nullopt);
  void UnlockSession(const std::string& password,
                     OptionalErrorCallback callback);

  mojo::ReceiverSet<mojom::Login> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::LacrosCleanupTriggeredObserver>
      lacros_cleanup_triggered_observers_;
  mojo::RemoteSet<mojom::ExternalLogoutRequestObserver>
      external_logout_request_observers_;
  base::ObserverList<ExternalLogoutDoneObserver>
      external_logout_done_observers_;

  base::WeakPtrFactory<LoginAsh> weak_factory_{this};
};

}  // namespace crosapi

namespace base {

template <>
struct ScopedObservationTraits<crosapi::LoginAsh,
                               crosapi::LoginAsh::ExternalLogoutDoneObserver> {
  static void AddObserver(
      crosapi::LoginAsh* source,
      crosapi::LoginAsh::ExternalLogoutDoneObserver* observer) {
    source->AddExternalLogoutDoneObserver(observer);
  }
  static void RemoveObserver(
      crosapi::LoginAsh* source,
      crosapi::LoginAsh::ExternalLogoutDoneObserver* observer) {
    source->RemoveExternalLogoutDoneObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_
