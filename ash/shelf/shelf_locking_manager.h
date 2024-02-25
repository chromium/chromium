// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LOCKING_MANAGER_H_
#define ASH_SHELF_SHELF_LOCKING_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/lock_state_observer.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class Shelf;

// ShelfLockingManager observes screen and session events to align the shelf at
// the bottom of the screen when the screen is locked.
class ASH_EXPORT ShelfLockingManager : public SessionObserver,
                                       public LockStateObserver {
 public:
  explicit ShelfLockingManager(Shelf* shelf);

  ShelfLockingManager(const ShelfLockingManager&) = delete;
  ShelfLockingManager& operator=(const ShelfLockingManager&) = delete;

  ~ShelfLockingManager() override;

  bool is_locked() const { return session_locked_ || screen_locked_; }
  void set_in_session_alignment(ShelfAlignment value) {
    in_session_alignment_ = value;
  }
  ShelfAlignment in_session_alignment() const { return in_session_alignment_; }
  ShelfAutoHideBehavior in_session_auto_hide_behavior() const {
    return in_session_auto_hide_behavior_;
  }

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // LockStateObserver:
  void OnLockStateEvent(EventType event) override;

 private:
  // Update the shelf state for session and screen lock changes.
  void UpdateLockedState();

  const raw_ptr<Shelf> shelf_;
  bool session_locked_ = false;
  bool screen_locked_ = false;

  // In session shelf alignment. This is used for setting shelf to/from
  // temporary `kBottomLocked` alignment.
  ShelfAlignment in_session_alignment_ = ShelfAlignment::kBottomLocked;

  // In session shelf auto hide behavior. This is used for work area insects
  // calculation for application windows.
  ShelfAutoHideBehavior in_session_auto_hide_behavior_ =
      ShelfAutoHideBehavior::kNever;

  ScopedSessionObserver scoped_session_observer_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LOCKING_MANAGER_H_
