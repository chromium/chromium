// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

// The ash-chrome implementation of the Login crosapi interface.
class LoginAsh : public mojom::Login {
 public:
  LoginAsh();
  LoginAsh(const LoginAsh&) = delete;
  LoginAsh& operator=(const LoginAsh&) = delete;
  ~LoginAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Login> receiver);

  // crosapi::mojom::Login:
  void LaunchManagedGuestSession(
      const absl::optional<std::string>& password,
      LaunchManagedGuestSessionCallback callback) override;
  void ExitCurrentSession(
      const absl::optional<std::string>& data_for_next_login_attempt,
      ExitCurrentSessionCallback callback) override;
  void FetchDataForNextLoginAttempt(
      FetchDataForNextLoginAttemptCallback callback) override;
  void LockManagedGuestSession(
      LockManagedGuestSessionCallback callback) override;
  void UnlockManagedGuestSession(
      const std::string& password,
      UnlockManagedGuestSessionCallback callback) override;
  void LockCurrentSession(LockCurrentSessionCallback callback) override;
  void UnlockCurrentSession(const std::string& password,
                            UnlockCurrentSessionCallback callback) override;
  void LaunchSamlUserSession(mojom::SamlUserSessionPropertiesPtr properties,
                             LaunchSamlUserSessionCallback callback) override;
  void LaunchSharedManagedGuestSession(
      const std::string& password,
      LaunchSharedManagedGuestSessionCallback callback) override;
  void EnterSharedSession(const std::string& password,
                          EnterSharedSessionCallback callback) override;
  void UnlockSharedSession(const std::string& password,
                           UnlockSharedSessionCallback callback) override;
  void EndSharedSession(EndSharedSessionCallback callback) override;
  void SetDataForNextLoginAttempt(
      const std::string& data_for_next_login_attempt,
      SetDataForNextLoginAttemptCallback callback) override;
  void AddLacrosCleanupTriggeredObserver(
      mojo::PendingRemote<mojom::LacrosCleanupTriggeredObserver> observer)
      override;

  mojo::RemoteSet<mojom::LacrosCleanupTriggeredObserver>&
  GetCleanupTriggeredObservers();

 private:
  void OnScreenLockerAuthenticate(
      base::OnceCallback<void(const absl::optional<std::string>&)> callback,
      bool success);
  void OnOptionalErrorCallbackComplete(
      base::OnceCallback<void(const absl::optional<std::string>&)> callback,
      const absl::optional<std::string>& error);
  absl::optional<std::string> CanLaunchSession();
  absl::optional<std::string> LockSession(
      absl::optional<user_manager::UserType> user_type = absl::nullopt);
  absl::optional<std::string> CanUnlockSession(
      absl::optional<user_manager::UserType> user_type = absl::nullopt);
  void UnlockSession(
      const std::string& password,
      base::OnceCallback<void(const absl::optional<std::string>&)> callback);

  mojo::ReceiverSet<mojom::Login> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::LacrosCleanupTriggeredObserver>
      lacros_cleanup_triggered_observers_;

  base::WeakPtrFactory<LoginAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOGIN_ASH_H_
