// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOGIN_STATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOGIN_STATE_ASH_H_

#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the LoginState crosapi interface.
class LoginStateAsh : public mojom::LoginState,
                      public session_manager::SessionManagerObserver {
 public:
  LoginStateAsh();
  LoginStateAsh(const LoginStateAsh&) = delete;
  LoginStateAsh& operator=(const LoginStateAsh&) = delete;
  ~LoginStateAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::LoginState> receiver);

  void AddObserver(mojo::PendingRemote<mojom::SessionStateChangedEventObserver>
                       observer) override;

  // crosapi::mojom::LoginState:
  void GetSessionState(GetSessionStateCallback callback) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  // Notifies all observers of the current session state.
  void NotifyObservers();

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::LoginState> receivers_;

  // This class supports any number of observers.
  mojo::RemoteSet<mojom::SessionStateChangedEventObserver> observers_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  // The cached session state value.
  mojom::SessionState session_state_ = mojom::SessionState::kUnknown;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOGIN_STATE_ASH_H_
