// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"

namespace ash {

ShelfLockingManager::ShelfLockingManager(Shelf* shelf)
    : shelf_(shelf), scoped_session_observer_(this) {
  DCHECK(shelf_);
  Shell::Get()->lock_state_controller()->AddObserver(this);
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  session_locked_ =
      controller->GetSessionState() != session_manager::SessionState::ACTIVE;
  screen_locked_ = controller->IsScreenLocked();
}

ShelfLockingManager::~ShelfLockingManager() {
  // |this| is destroyed after LockStateController for the primary display.
  if (Shell::Get()->lock_state_controller())
    Shell::Get()->lock_state_controller()->RemoveObserver(this);
}

void ShelfLockingManager::OnLockStateChanged(bool locked) {
  screen_locked_ = locked;
  UpdateLockedState();
}

void ShelfLockingManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  session_locked_ = state != session_manager::SessionState::ACTIVE;
  UpdateLockedState();
}

void ShelfLockingManager::OnLockStateEvent(EventType event) {
  // Lock when the animation starts, ignoring pre-lock. There's no unlock event.
  screen_locked_ |= event == EVENT_LOCK_ANIMATION_STARTED;
  UpdateLockedState();
}

void ShelfLockingManager::UpdateLockedState() {
  const ShelfAlignment alignment = shelf_->alignment();
  if (is_locked() && alignment != ShelfAlignment::kBottomLocked) {
    in_session_auto_hide_behavior_ = shelf_->auto_hide_behavior();
    in_session_alignment_ = alignment;
    shelf_->SetAlignment(ShelfAlignment::kBottomLocked);
  } else if (!is_locked() && alignment == ShelfAlignment::kBottomLocked) {
    in_session_auto_hide_behavior_ = ShelfAutoHideBehavior::kNever;
    shelf_->SetAlignment(in_session_alignment_);
  }
}

}  // namespace ash
