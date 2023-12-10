// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_
#define ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ui {
class CompositorObserver;
}

namespace ash {

class LockOnSuspendUsage;

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

  PowerEventObserver(const PowerEventObserver&) = delete;
  PowerEventObserver& operator=(const PowerEventObserver&) = delete;

  ~PowerEventObserver() override;

  // Called by D-Bus when the current switches state is successfully obtained.
  void OnGetSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> result);

  // Called by the WebUIScreenLocker when all the lock screen animations have
  // completed.  This really should be implemented via an observer but since
  // ash/ isn't allowed to depend on chrome/ we need to have the
  // WebUIScreenLocker reach into ash::Shell to make this call.
  void OnLockAnimationsComplete();

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDoneEx(const power_manager::SuspendDone& proto) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  // SessionObserver overrides:
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnLockStateChanged(bool locked) override;

  // Sets whether the device is projecting (docked).  This is used in along with
  // lid state to lock the device.
  void SetIsProjecting(bool is_projecting);

 private:
  friend class PowerEventObserverTestApi;

  // Locks device when lid is closed, and device is not projecting (docked), if
  // user/policy settings configured.
  void MaybeLockOnLidClose(bool is_projecting);

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

  // Starts |wait_for_external_display_timer_|.
  void StartExternalDisplayTimer();

  LockState lock_state_ = LockState::kUnlocked;
  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::OPEN;

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

  std::unique_ptr<LockOnSuspendUsage> lock_on_suspend_usage_;

  // Amount of time (in seconds) to wait for external displays when a display
  // mode change occurs and the lid is closed.
  int defer_external_display_timeout_s_ = 0;
  base::OneShotTimer wait_for_external_display_timer_;

  base::WeakPtrFactory<PowerEventObserver> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_EVENT_OBSERVER_H_
