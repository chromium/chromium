// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_state_ash.h"

#include "base/trace_event/trace_event.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace crosapi {

namespace {

// Not all session states are exposed. Session states which are not exposed will
// be mapped to the nearest logical state. The mapping is as follows:
// UNKNOWN              -> kUnknown
// OOBE                 -> kInOobeScreen
// LOGIN_PRIMARY        -> kInLoginScreen
// LOGIN_SECONDARY      -> kInLoginScreen
// LOGGED_IN_NOT_ACTIVE -> kInLoginScreen
// ACTIVE               -> kInSession
// LOCKED               -> kInLockScreen
mojom::SessionState ToMojo(session_manager::SessionState state) {
  switch (state) {
    case session_manager::SessionState::UNKNOWN:
      return mojom::SessionState::kUnknown;
    case session_manager::SessionState::OOBE:
      return mojom::SessionState::kInOobeScreen;
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return mojom::SessionState::kInLoginScreen;
    case session_manager::SessionState::ACTIVE:
      return mojom::SessionState::kInSession;
    case session_manager::SessionState::LOCKED:
      return mojom::SessionState::kInLockScreen;
    case session_manager::SessionState::RMA:
      return mojom::SessionState::kInRmaScreen;
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::SessionState::kUnknown;
}

}  // namespace

LoginStateAsh::LoginStateAsh() {
  // SessionManager may be unset in tests.
  if (session_manager::SessionManager::Get()) {
    session_state_ =
        ToMojo(session_manager::SessionManager::Get()->session_state());
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
  }
}
LoginStateAsh::~LoginStateAsh() = default;

void LoginStateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::LoginState> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LoginStateAsh::AddObserver(
    mojo::PendingRemote<mojom::SessionStateChangedEventObserver> observer) {
  mojo::Remote<mojom::SessionStateChangedEventObserver> remote(
      std::move(observer));

  // Store the observer for future notifications.
  observers_.Add(std::move(remote));
}

void LoginStateAsh::GetSessionState(GetSessionStateCallback callback) {
  std::move(callback).Run(
      mojom::GetSessionStateResult::NewSessionState(session_state_));
}

void LoginStateAsh::OnSessionStateChanged() {
  TRACE_EVENT0("login", "LoginStateAsh::OnSessionStateChanged");
  mojom::SessionState new_state =
      ToMojo(session_manager::SessionManager::Get()->session_state());

  // |session_manager::SessionState| changed but the mapped |mojo::SessionState|
  // did not.
  if (session_state_ == new_state)
    return;

  session_state_ = new_state;

  LoginStateAsh::NotifyObservers();
}

void LoginStateAsh::NotifyObservers() {
  for (auto& observer : observers_) {
    observer->OnSessionStateChanged(session_state_);
  }
}

}  // namespace crosapi
