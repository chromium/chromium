// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LOCKING_MANAGER_H_
#define ASH_SHELF_SHELF_LOCKING_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_observer.h"
#include "ash/wm/lock_state_observer.h"

namespace ash {

class Shelf;

// ShelfLockingManager observes screen and session events to align the shelf at
// the bottom of the screen when the screen is locked.
class ASH_EXPORT ShelfLockingManager : public SessionObserver,
                                       public LockStateObserver {
 public:
  explicit ShelfLockingManager(Shelf* shelf);
  ~ShelfLockingManager() override;

  bool is_locked() const { return session_locked_ || screen_locked_; }
  void set_stored_alignment(ShelfAlignment value) { stored_alignment_ = value; }

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
  ShelfAlignment stored_alignment_;

  ScopedSessionObserver scoped_session_observer_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLockingManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LOCKING_MANAGER_H_
