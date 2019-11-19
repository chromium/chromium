// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_
#define ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ui {
class CompositorObserver;
}

namespace ash {

// A class that observes power-management-related events - in particular, it
// observes the device suspend state and updates display states accordingly.
// When the device suspends, it suspends all displays and stops compositing.
// On resume, displays are resumed, and compositing is started again.
// During suspend, it ensures compositing is not stopped prematurely if the
// screen is being locked during suspend - display compositing will not be
// stopped before:
//  1. lock screen window is shown
//  2. wallpaper changes due to screen lock are finished
//  3. the compositor goes through at least two compositing cycles after the
//     screen lock
// This is done to ensure that displays have picked up frames from after the
// screen was locked. Without this, displays might initially show
// pre-screen-lock frames when resumed.
// For example, see https://crbug.com/807511.
class ASH_EXPORT PowerEventObserver
    : public chromeos::PowerManagerClient::Observer,
      public SessionObserver {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  PowerEventObserver();
  ~PowerEventObserver() override;

  // Called by the WebUIScreenLocker when all the lock screen animations have
  // completed.  This really should be implemented via an observer but since
  // ash/ isn't allowed to depend on chrome/ we need to have the
  // WebUIScreenLocker reach into ash::Shell to make this call.
  void OnLockAnimationsComplete();

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // SessionObserver overrides:
  void OnLockStateChanged(bool locked) override;

 private:
  friend class PowerEventObserverTestApi;

  enum class LockState {
    // Screen lock has not been requested, nor detected.
    kUnlocked,
    // Screen lock has been requested, or detected, but screen lock has not
    // reported that it finished showing.
    kLocking,
    // Screen has been locked, but all compositors might not have yet picked up
    // locked screen state - |compositor_watcher_| is observing compositors,
    // waiting for them to become ready to suspend.
    kLockedCompositingPending,
    // Screen is locked, and displays have picked up lock screen changes - it
    // should be safe to stop compositing and start suspend at this time.
    kLocked,
  };

  // Sets all root window compositors' visibility to true.
  void StartRootWindowCompositors();

  // Sets all root window compositors' visibility to false, and then suspends
  // displays. It will run unblock suspend via |block_suspend_token_| once
  // displays are suspended. This should only be called when it's safe to stop
  // compositing - either if the screen is not expected to get locked, or all
  // compositors have gone through compositing cycle after the screen was
  // locked.
  void StopCompositingAndSuspendDisplays();

  // If any of the root windows have pending wallpaper animations, it stops
  // them - this is used to stop wallpaper animations during suspend, and thus
  // improve the suspend time (given that suspend will be delayed until the
  // wallpaper animations finish).
  void EndPendingWallpaperAnimations();

  // Callback run by |compositor_watcher_| when it detects that composting
  // can be stopped for all root windows when device suspends.
  void OnCompositorsReadyForSuspend();

  LockState lock_state_ = LockState::kUnlocked;

  ScopedSessionObserver session_observer_;

  // Whether the device is suspending.
  bool suspend_in_progress_ = false;

  // Used to observe compositing state after screen lock to detect when display
  // compositors are in state in which it's safe to proceed with suspend.
  std::unique_ptr<ui::CompositorObserver> compositor_watcher_;

  // Token set when device suspend is delayed due to a screen lock - suspend
  // should be continued when the screen lock finishes showing and display
  // compositors pick up screen lock changes. All compositors should be stopped
  // prior to unblocking and clearing this - call
  // StopCompositingAndSuspendDisplays(). This will only be set while the device
  // is suspending.
  base::UnguessableToken block_suspend_token_;

  DISALLOW_COPY_AND_ASSIGN(PowerEventObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_
