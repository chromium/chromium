// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/tray_action.h"

#include <utility>

#include "ash/lock_screen_action/lock_screen_note_display_state_handler.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/events/devices/stylus_state.h"

namespace ash {

TrayAction::TrayAction(BacklightsForcedOffSetter* backlights_forced_off_setter)
    : backlights_forced_off_setter_(backlights_forced_off_setter) {
  stylus_observation_.Observe(ui::DeviceDataManager::GetInstance());
}

TrayAction::~TrayAction() = default;

void TrayAction::AddObserver(TrayActionObserver* observer) {
  observers_.AddObserver(observer);
}

void TrayAction::RemoveObserver(TrayActionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TrayAction::BindReceiver(
    mojo::PendingReceiver<mojom::TrayAction> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojom::TrayActionState TrayAction::GetLockScreenNoteState() const {
  if (!tray_action_client_)
    return mojom::TrayActionState::kNotAvailable;
  return lock_screen_note_state_;
}

bool TrayAction::IsLockScreenNoteActive() const {
  return GetLockScreenNoteState() == mojom::TrayActionState::kActive;
}

void TrayAction::SetClient(
    mojo::PendingRemote<mojom::TrayActionClient> tray_action_client,
    mojom::TrayActionState lock_screen_note_state) {
  mojom::TrayActionState old_lock_screen_note_state = GetLockScreenNoteState();

  if (tray_action_client) {
    tray_action_client_.Bind(std::move(tray_action_client));

    // Makes sure the state is updated in case the connection is lost.
    tray_action_client_.set_disconnect_handler(base::BindOnce(
        &TrayAction::SetClient, base::Unretained(this), mojo::NullRemote(),
        mojom::TrayActionState::kNotAvailable));
    lock_screen_note_state_ = lock_screen_note_state;

    lock_screen_note_display_state_handler_ =
        std::make_unique<LockScreenNoteDisplayStateHandler>(
            backlights_forced_off_setter_);
  } else {
    tray_action_client_.reset();
    lock_screen_note_display_state_handler_.reset();
  }

  // Setting action handler value can change effective state - notify observers
  // if that was the case.
  if (GetLockScreenNoteState() != old_lock_screen_note_state)
    NotifyLockScreenNoteStateChanged();
}

void TrayAction::UpdateLockScreenNoteState(mojom::TrayActionState state) {
  if (state == lock_screen_note_state_)
    return;

  lock_screen_note_state_ = state;

  if (lock_screen_note_state_ == mojom::TrayActionState::kNotAvailable)
    lock_screen_note_display_state_handler_->Reset();

  // If the client is not set, the effective state has not changed, so no need
  // to notify observers of a state change.
  if (tray_action_client_)
    NotifyLockScreenNoteStateChanged();
}

void TrayAction::RequestNewLockScreenNote(mojom::LockScreenNoteOrigin origin) {
  if (GetLockScreenNoteState() != mojom::TrayActionState::kAvailable)
    return;

  // An action state can be kAvailable only if |tray_action_client_| is set.
  DCHECK(tray_action_client_);
  tray_action_client_->RequestNewLockScreenNote(origin);
}

void TrayAction::CloseLockScreenNote(mojom::CloseLockScreenNoteReason reason) {
  if (tray_action_client_)
    tray_action_client_->CloseLockScreenNote(reason);
}

void TrayAction::OnStylusStateChanged(ui::StylusState state) {
  if (state == ui::StylusState::REMOVED)
    lock_screen_note_display_state_handler_->AttemptNoteLaunchForStylusEject();
}

void TrayAction::FlushMojoForTesting() {
  if (tray_action_client_)
    tray_action_client_.FlushForTesting();
}

void TrayAction::NotifyLockScreenNoteStateChanged() {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(GetLockScreenNoteState());
}

}  // namespace ash
