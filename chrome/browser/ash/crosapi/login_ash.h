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
  void AddExternalLogoutRequestObserver(
      mojo::PendingRemote<mojom::ExternalLogoutRequestObserver> observer)
      override;
  void NotifyOnExternalLogoutDone() override;
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

  // Adds an observer for the external logout done events.
  void AddExternalLogoutDoneObserver(ExternalLogoutDoneObserver* observer);
  // Required for the below `base::ObserverList`:
  void RemoveExternalLogoutDoneObserver(ExternalLogoutDoneObserver* observer);
  // Notifies the external logout observers with the
  // `login.onRequestExternalLogout` event. It is called from the login screen
  // extension running on the lock screen (ash-chrome). The in-session extension
  // listens for the dispatched event.
  void NotifyOnRequestExternalLogout();

 private:
  mojo::ReceiverSet<mojom::Login> receivers_;

  // Support any number of observers.
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
