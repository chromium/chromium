// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LOCKING_MANAGER_H_
#define ASH_SHELF_SHELF_LOCKING_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/lock_state_observer.h"

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
  void set_stored_alignment(ShelfAlignment value) { stored_alignment_ = value; }
  ShelfAlignment stored_alignment() const { return stored_alignment_; }

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // LockStateObserver:
  void OnLockStateEvent(EventType event) override;

 private:
  // Update the shelf state for session and screen lock changes.
  void UpdateLockedState();

  Shelf* const shelf_;
  bool session_locked_ = false;
  bool screen_locked_ = false;
  ShelfAlignment stored_alignment_ = ShelfAlignment::kBottomLocked;

  ScopedSessionObserver scoped_session_observer_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LOCKING_MANAGER_H_
