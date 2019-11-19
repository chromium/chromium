// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_event_observer.h"

#include <map>
#include <utility>

#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/lock_state_observer.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

namespace {

void OnSuspendDisplaysCompleted(base::UnguessableToken token, bool status) {
  chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
}

// Returns whether the screen should be locked when device is suspended.
bool ShouldLockOnSuspend() {
  SessionControllerImpl* controller = ash::Shell::Get()->session_controller();

  return controller->ShouldLockScreenAutomatically() &&
         controller->CanLockScreen();
}

// One-shot class that runs a callback after all compositors start and
// complete two compositing cycles. This should ensure that buffer swap with the
// current UI has happened.
// After the first compositing cycle, the display compositor starts drawing the
// UI changes, and schedules a buffer swap. Given that the display compositor
// will not start drawing the next frame before the previous swap happens, when
// the second compositing cycle ends, it should be safe to assume the required
// buffer swap happened at that point.
// Note that the compositor watcher will wait for any pending wallpaper
// animation for a root window to finish before it starts observing compositor
// cycles, to ensure it picks up wallpaper state from after the animation ends,
// and avoids issues like https://crbug.com/820436.
class CompositorWatcher : public ui::CompositorObserver {
 public:
  // |callback| - called when all visible root window compositors complete
  //     required number of compositing cycles. It will not be called after
  //     CompositorWatcher instance is deleted, nor from the CompositorWatcher
  //     destructor.
  explicit CompositorWatcher(base::OnceClosure callback)
      : callback_(std::move(callback)), compositor_observer_(this) {
    Start();
  }
  ~CompositorWatcher() override = default;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    if (!pending_compositing_.count(compositor) ||
        pending_compositing_[compositor].state !=
            CompositingState::kWaitingForCommit) {
      return;
    }
    pending_compositing_[compositor].state =
        CompositingState::kWaitingForStarted;
  }
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override {
    if (!pending_compositing_.count(compositor) ||
        pending_compositing_[compositor].state !=
            CompositingState::kWaitingForStarted) {
      return;
    }
    pending_compositing_[compositor].state = CompositingState::kWaitingForEnded;
  }
  void OnCompositingEnded(ui::Compositor* compositor) override {
    if (!pending_compositing_.count(compositor))
      return;
    CompositorInfo& compositor_info = pending_compositing_[compositor];
    if (compositor_info.state != CompositingState::kWaitingForEnded)
      return;

    compositor_info.observed_cycles++;
    if (compositor_info.observed_cycles < kRequiredCompositingCycles) {
      compositor_info.state = CompositingState::kWaitingForCommit;
      compositor->ScheduleDraw();
      return;
    }

    compositor_observer_.Remove(compositor);
    pending_compositing_.erase(compositor);

    RunCallbackIfAllCompositingEnded();
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    compositor_observer_.Remove(compositor);
    pending_compositing_.erase(compositor);

    RunCallbackIfAllCompositingEnded();
  }

 private:
  // CompositorWatcher observes compositors for compositing events, in order to
  // determine whether compositing cycles end for all root window compositors.
  // This enum is used to track this cycle. Compositing goes through the
  // following states: DidCommit -> CompositingStarted -> CompositingEnded.
  enum class CompositingState {
    kWaitingForWallpaperAnimation,
    kWaitingForCommit,
    kWaitingForStarted,
    kWaitingForEnded,
  };

  struct CompositorInfo {
    // State of the current compositing cycle.
    CompositingState state = CompositingState::kWaitingForCommit;

    // Number of observed compositing cycles.
    int observed_cycles = 0;
  };

  // Number of compositing cycles that have to complete for each compositor
  // in order for a CompositorWatcher to run the callback.
  static constexpr int kRequiredCompositingCycles = 2;

  // Starts observing all visible root window compositors.
  void Start() {
    for (aura::Window* window : Shell::GetAllRootWindows()) {
      ui::Compositor* compositor = window->GetHost()->compositor();
      if (!compositor->IsVisible())
        continue;

      DCHECK(!pending_compositing_.count(compositor));

      compositor_observer_.Add(compositor);
      pending_compositing_[compositor].state =
          CompositingState::kWaitingForWallpaperAnimation;

      WallpaperWidgetController* wallpaper_widget_controller =
          RootWindowController::ForWindow(window)
              ->wallpaper_widget_controller();
      if (wallpaper_widget_controller->IsAnimating()) {
        wallpaper_widget_controller->AddAnimationEndCallback(
            base::BindOnce(&CompositorWatcher::StartObservingCompositing,
                           weak_ptr_factory_.GetWeakPtr(), compositor));
      } else {
        StartObservingCompositing(compositor);
      }
    }

    // Post task to make sure callback is not invoked synchronously as watcher
    // is started.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CompositorWatcher::RunCallbackIfAllCompositingEnded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when the wallpaper animations end for the root window associated
  // with the compositor. It starts observing the compositor's compositing
  // cycles.
  void StartObservingCompositing(ui::Compositor* compositor) {
    if (!pending_compositing_.count(compositor) ||
        pending_compositing_[compositor].state !=
            CompositingState::kWaitingForWallpaperAnimation) {
      return;
    }

    pending_compositing_[compositor].state =
        CompositingState::kWaitingForCommit;
    // Schedule a draw to force at least one more compositing cycle.
    compositor->ScheduleDraw();
  }

  // If all observed root window compositors have gone through a compositing
  // cycle, runs |callback_|.
  void RunCallbackIfAllCompositingEnded() {
    if (pending_compositing_.empty() && callback_)
      std::move(callback_).Run();
  }

  base::OnceClosure callback_;

  // Per-compositor compositing state tracked by |this|. The map will
  // not contain compositors that were not visible at the time the
  // CompositorWatcher was started - the main purpose of tracking compositing
  // state is to determine whether compositors can be safely stopped (i.e. their
  // visibility set to false), so there should be no need for tracking
  // compositors that were hidden to start with.
  std::map<ui::Compositor*, CompositorInfo> pending_compositing_;
  ScopedObserver<ui::Compositor, ui::CompositorObserver> compositor_observer_;

  base::WeakPtrFactory<CompositorWatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CompositorWatcher);
};

}  // namespace

PowerEventObserver::PowerEventObserver()
    : lock_state_(Shell::Get()->session_controller()->IsScreenLocked()
                      ? LockState::kLocked
                      : LockState::kUnlocked),
      session_observer_(this) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

PowerEventObserver::~PowerEventObserver() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerEventObserver::OnLockAnimationsComplete() {
  VLOG(1) << "Screen locker animations have completed.";
  if (lock_state_ != LockState::kLocking)
    return;

  lock_state_ = LockState::kLockedCompositingPending;

  // If suspending, run pending animations to the end immediately, as there is
  // no point in waiting for them to finish given that the device is suspending.
  if (block_suspend_token_)
    EndPendingWallpaperAnimations();

  // The |compositor_watcher_| is owned by this, and the callback passed to it
  // won't be called  after |compositor_watcher_|'s destruction, so
  // base::Unretained is safe here.
  compositor_watcher_ = std::make_unique<CompositorWatcher>(
      base::BindOnce(&PowerEventObserver::OnCompositorsReadyForSuspend,
                     base::Unretained(this)));
}

void PowerEventObserver::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  suspend_in_progress_ = true;

  block_suspend_token_ = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(block_suspend_token_,
                                                    "PowerEventObserver");

  // Stop compositing immediately if
  // * the screen lock flow has already completed
  // * screen is not locked, and should remain unlocked during suspend
  if (lock_state_ == LockState::kLocked ||
      (lock_state_ == LockState::kUnlocked && !ShouldLockOnSuspend())) {
    StopCompositingAndSuspendDisplays();
  } else {
    // If screen is getting locked during suspend, delay suspend until screen
    // lock finishes, and post-lock frames get picked up by display compositors.
    if (lock_state_ == LockState::kUnlocked) {
      VLOG(1) << "Requesting screen lock from PowerEventObserver";
      lock_state_ = LockState::kLocking;
      Shell::Get()->lock_state_controller()->LockWithoutAnimation();
    } else if (lock_state_ != LockState::kLocking) {
      // If the screen is still being locked (i.e. in kLocking state),
      // EndPendingWallpaperAnimations() will be called in
      // OnLockAnimationsComplete().
      EndPendingWallpaperAnimations();
    }
  }
}

void PowerEventObserver::SuspendDone(const base::TimeDelta& sleep_duration) {
  suspend_in_progress_ = false;

  Shell::Get()->display_configurator()->ResumeDisplays();
  Shell::Get()->system_tray_model()->clock()->NotifyRefreshClock();

  // If the suspend request was being blocked while waiting for the lock
  // animation to complete, clear the blocker since the suspend has already
  // completed.  This prevents rendering requests from being blocked after a
  // resume if the lock screen took too long to show.
  block_suspend_token_ = {};

  StartRootWindowCompositors();
}

void PowerEventObserver::OnLockStateChanged(bool locked) {
  if (locked) {
    lock_state_ = LockState::kLocking;

    // The screen is now locked but the pending suspend, if any, will be blocked
    // until all the animations have completed.
    if (block_suspend_token_)
      VLOG(1) << "Screen locked due to suspend";
    else
      VLOG(1) << "Screen locked without suspend";
  } else {
    lock_state_ = LockState::kUnlocked;
    compositor_watcher_.reset();

    if (suspend_in_progress_) {
      LOG(WARNING) << "Screen unlocked during suspend";
      // If screen gets unlocked during suspend, which could theoretically
      // happen if the user initiated unlock just as device started unlocking
      // (though, it seems unlikely this would be encountered in practice),
      // relock the device if required. Otherwise, if suspend is blocked due to
      // screen locking, unblock it.
      if (ShouldLockOnSuspend()) {
        lock_state_ = LockState::kLocking;
        Shell::Get()->lock_state_controller()->LockWithoutAnimation();
      } else if (block_suspend_token_) {
        StopCompositingAndSuspendDisplays();
      }
    }
  }
}

void PowerEventObserver::StartRootWindowCompositors() {
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    if (!compositor->IsVisible())
      compositor->SetVisible(true);
  }
}

void PowerEventObserver::StopCompositingAndSuspendDisplays() {
  DCHECK(block_suspend_token_);
  DCHECK(!compositor_watcher_.get());
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    compositor->SetVisible(false);
  }

  ui::UserActivityDetector::Get()->OnDisplayPowerChanging();

  Shell::Get()->display_configurator()->SuspendDisplays(
      base::Bind(&OnSuspendDisplaysCompleted, block_suspend_token_));
  block_suspend_token_ = {};
}

void PowerEventObserver::EndPendingWallpaperAnimations() {
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    WallpaperWidgetController* wallpaper_widget_controller =
        RootWindowController::ForWindow(window)->wallpaper_widget_controller();
    if (wallpaper_widget_controller->IsAnimating())
      wallpaper_widget_controller->EndPendingAnimation();
  }
}

void PowerEventObserver::OnCompositorsReadyForSuspend() {
  compositor_watcher_.reset();
  lock_state_ = LockState::kLocked;

  if (block_suspend_token_)
    StopCompositingAndSuspendDisplays();
}

}  // namespace ash
