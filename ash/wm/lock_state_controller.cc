// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/cancel_mode.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/shutdown_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_reason.h"
#include "ash/utility/occlusion_tracker_pauser.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_animator_impl.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"

#define UMA_HISTOGRAM_LOCK_TIMES(name, sample)                    \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::Milliseconds(1), \
                             base::Seconds(50), 100)

// TODO(b/228873153): Remove after figuring out the root cause of the bug
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

// ASan/TSan/MSan instrument each memory access. This may slow the execution
// down significantly.
#if defined(MEMORY_SANITIZER)
// For MSan the slowdown depends heavily on the value of msan_track_origins GYP
// flag. The multiplier below corresponds to msan_track_origins=1.
constexpr int kTimeoutMultiplier = 6;
#elif defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
constexpr int kTimeoutMultiplier = 2;
#else
constexpr int kTimeoutMultiplier = 1;
#endif

constexpr int kMaxShutdownSoundDurationMs = 1500;

// Amount of time to wait for our lock requests to be honored before giving up.
constexpr base::TimeDelta kLockFailTimeout =
    base::Seconds(8 * kTimeoutMultiplier);

// Amount of time to wait for our post lock animation before giving up.
constexpr base::TimeDelta kPostLockFailTimeout =
    base::Seconds(2 * kTimeoutMultiplier);

// Additional time to wait after starting the fast-close shutdown animation
// before actually requesting shutdown, to give the animation time to finish.
constexpr base::TimeDelta kShutdownRequestDelay = base::Milliseconds(50);

}  // namespace

// static
const int LockStateController::kPreLockContainersMask =
    SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
    SessionStateAnimator::SHELF;

LockStateController::LockStateController(
    ShutdownController* shutdown_controller,
    PrefService* local_state)
    : animator_(new SessionStateAnimatorImpl()),
      shutdown_controller_(shutdown_controller),
      scoped_session_observer_(this),
      local_state_(local_state) {
  DCHECK(shutdown_controller_);
  Shell::GetPrimaryRootWindow()->GetHost()->AddObserver(this);
  // |local_state_| could be null in tests.
  if (local_state_) {
    // If kLoginShutdownTimestampPrefName is registered, check the last recorded
    // login shutdown timestamp in local state prefs, in case device was shut
    // down using shelf button.
    auto* login_shutdown_timestamp_pref =
        local_state_->FindPreference(prefs::kLoginShutdownTimestampPrefName);
    if (login_shutdown_timestamp_pref &&
        !login_shutdown_timestamp_pref->IsDefaultValue()) {
      base::Time last_recorded_login_shutdown_timestamp =
          base::ValueToTime(login_shutdown_timestamp_pref->GetValue()).value();
      base::TimeDelta duration = base::DefaultClock::GetInstance()->Now() -
                                 last_recorded_login_shutdown_timestamp;
      // Report time delta even if it exceeds histogram limit, to better
      // understand fraction of users using the feature.
      base::UmaHistogramLongTimes(
          "Ash.Shelf.ShutdownConfirmationBubble.TimeToNextBoot."
          "LoginShutdownToPowerUpDuration",
          duration);

      // Reset to the default value after the value is recorded.
      local_state_->ClearPref(prefs::kLoginShutdownTimestampPrefName);
    }
  }
}

LockStateController::~LockStateController() {
  Shell::GetPrimaryRootWindow()->GetHost()->RemoveObserver(this);
}

// static
void LockStateController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kLoginShutdownTimestampPrefName,
                             base::Time());
}

void LockStateController::AddObserver(LockStateObserver* observer) {
  observers_.AddObserver(observer);
}

void LockStateController::RemoveObserver(LockStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LockStateController::StartLockAnimation() {
  if (animating_lock_)
    return;

  animating_lock_ = true;
  StoreUnlockedProperties();
  VLOG(1) << "StartLockAnimation";
  PreLockAnimation(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, true);
  DispatchCancelMode();
  OnLockStateEvent(LockStateObserver::EVENT_PRELOCK_ANIMATION_STARTED);
}

void LockStateController::StartShutdownAnimation(ShutdownReason reason) {
  shutdown_reason_ = reason;

  Shell* shell = Shell::Get();
  // Hide cursor, but let it reappear if the mouse moves.
  if (shell->cursor_manager())
    shell->cursor_manager()->HideCursor();

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
      SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartPreShutdownAnimationTimer();
}

void LockStateController::LockWithoutAnimation() {
  VLOG(1) << "LockWithoutAnimation : "
          << "animating_unlock_: " << static_cast<int>(animating_unlock_)
          << ", animating_lock_: " << static_cast<int>(animating_lock_);
  if (animating_unlock_) {
    CancelUnlockAnimation();
    // One would expect a call to
    // `Shell::Get()->session_controller()->LockScreen()` at this point,
    // however, when execution reaches here, if:
    //
    // We were running the animations started as part of
    // StartUnlockAnimationBeforeLockUIDestroyed, `session_manager` still
    // considers the screen to be locked, as we've only executed the part of the
    // animations done before the lock screen UI is destroyed.
    //
    // We were running the animations started as part of
    // StartUnlockAnimationAfterLockUIDestroyed, `session_manager` would
    // consider the session to be unlocked, and thus we lock it again as part of
    // UnlockAnimationAfterLockUIDestroyedFinished.
    return;
  }
  if (animating_lock_)
    return;
  animating_lock_ = true;
  post_lock_immediate_animation_ = true;
  animator_->StartAnimation(kPreLockContainersMask,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  OnLockStateEvent(LockStateObserver::EVENT_LOCK_ANIMATION_STARTED);
  Shell::Get()->session_controller()->LockScreen();
}

bool LockStateController::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool LockStateController::ShutdownRequested() {
  return shutting_down_;
}

void LockStateController::CancelLockAnimation() {
  VLOG(1) << "CancelLockAnimation";
  animating_lock_ = false;
  Shell::Get()->wallpaper_controller()->RestoreWallpaperBlurForLockState(
      saved_blur_);
  auto next_animation_starter =
      base::BindOnce(&LockStateController::LockAnimationCancelled,
                     weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(std::move(next_animation_starter));

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_UNDO_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);
  animation_sequence->StartAnimation(
      SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);
  AnimateWallpaperHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
      animation_sequence);

  animation_sequence->EndSequence();
}

void LockStateController::CancelUnlockAnimation() {
  VLOG(1) << "CancelUnlockAnimation";
  pb_pressed_during_unlock_ = true;
}

bool LockStateController::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning();
}

void LockStateController::CancelShutdownAnimation() {
  if (!CanCancelShutdownAnimation())
    return;

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
      SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
  pre_shutdown_timer_.Stop();
}

void LockStateController::RequestShutdown(ShutdownReason reason) {
  if (shutting_down_)
    return;

  shutting_down_ = true;
  shutdown_reason_ = reason;

  if (reason == ShutdownReason::LOGIN_SHUT_DOWN_BUTTON) {
    base::Time now_timestamp = base::DefaultClock::GetInstance()->Now();
    local_state_->SetTime(prefs::kLoginShutdownTimestampPrefName,
                          now_timestamp);
  }

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->HideCursor();
  cursor_manager->LockCursor();

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
      SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  StartRealShutdownTimer(true);
}

void LockStateController::OnUnlockAnimationBeforeLockUIDestroyedFinished() {
  if (pb_pressed_during_unlock_) {
    // Power button was pressed during the unlock animation and
    // CancelUnlockAnimation was called, restore UI elements to previous state
    // immediately.
    animator_->StartAnimation(SessionStateAnimator::SHELF,
                              SessionStateAnimator::ANIMATION_FADE_IN,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                              SessionStateAnimator::ANIMATION_UNDO_LIFT,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    // We aborted, so we are not animating anymore.
    animating_unlock_ = false;
  }
  std::move(start_unlock_callback_).Run(pb_pressed_during_unlock_);
  pb_pressed_during_unlock_ = false;
}

void LockStateController::OnLockScreenHide(
    SessionStateAnimator::AnimationCallback callback) {
  start_unlock_callback_ = std::move(callback);
  StartUnlockAnimationBeforeLockUIDestroyed(base::BindOnce(
      &LockStateController::OnUnlockAnimationBeforeLockUIDestroyedFinished,
      weak_ptr_factory_.GetWeakPtr()));
}

void LockStateController::SetLockScreenDisplayedCallback(
    base::OnceClosure callback) {
  DCHECK(lock_screen_displayed_callback_.is_null());
  if (system_is_locked_ && !animating_lock_)
    std::move(callback).Run();
  else
    lock_screen_displayed_callback_ = std::move(callback);
}

void LockStateController::OnHostCloseRequested(aura::WindowTreeHost* host) {
  Shell::Get()->session_controller()->RequestSignOut();
}

void LockStateController::OnChromeTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  // This is also the case when the user signs off.
  if (!shutting_down_) {
    shutting_down_ = true;
    ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
    cursor_manager->HideCursor();
    cursor_manager->LockCursor();
    animator_->StartAnimation(SessionStateAnimator::kAllNonRootContainersMask,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
}

void LockStateController::OnLockStateChanged(bool locked) {
  // Unpause if lock animations didn't start and ends in 3 seconds.
  constexpr base::TimeDelta kPauseTimeout = base::Seconds(3);

  DCHECK((lock_fail_timer_.IsRunning() && lock_duration_timer_ != nullptr) ||
         (!lock_fail_timer_.IsRunning() && lock_duration_timer_ == nullptr));
  VLOG(1) << "OnLockStateChanged called with locked: " << locked
          << ", shutting_down_: " << shutting_down_
          << ", system_is_locked_: " << system_is_locked_
          << ", lock_fail_timer_.IsRunning(): " << lock_fail_timer_.IsRunning()
          << ", animating_unlock_: " << static_cast<int>(animating_unlock_)
          << ", animating_lock_: " << static_cast<int>(animating_lock_);

  if (shutting_down_ || (system_is_locked_ == locked))
    return;

  system_is_locked_ = locked;

  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      kPauseTimeout);

  if (locked) {
    StartPostLockAnimation();

    lock_fail_timer_.Stop();

    if (lock_duration_timer_) {
      UMA_HISTOGRAM_LOCK_TIMES("Ash.WindowManager.Lock.Success",
                               lock_duration_timer_->Elapsed());
      lock_duration_timer_.reset();
    }
  } else {
    StartUnlockAnimationAfterLockUIDestroyed();
  }
}

void LockStateController::OnLockFailTimeout() {
  UMA_HISTOGRAM_LOCK_TIMES("Ash.WindowManager.Lock.Timeout",
                           lock_duration_timer_->Elapsed());
  lock_duration_timer_.reset();
  DCHECK(!system_is_locked_);

  // b/228873153: Here we use `LOG(ERROR)` instead of `LOG(FATAL)` because it
  // seems like certain users are hitting this timeout causing chrome to crash
  // and be restarted from session manager without `--login-manager`
  LOG(ERROR) << "Screen lock took too long; Signing out";
  base::debug::DumpWithoutCrashing();
  Shell::Get()->session_controller()->RequestSignOut();
}

void LockStateController::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN),
      this, &LockStateController::OnPreShutdownAnimationTimeout);
}

void LockStateController::OnPreShutdownAnimationTimeout() {
  VLOG(1) << "OnPreShutdownAnimationTimeout";
  shutting_down_ = true;

  Shell* shell = Shell::Get();
  if (shell->cursor_manager())
    shell->cursor_manager()->HideCursor();

  StartRealShutdownTimer(false);
}

void LockStateController::StartRealShutdownTimer(bool with_animation_time) {
  base::TimeDelta duration = kShutdownRequestDelay;
  if (with_animation_time) {
    duration +=
        animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  }
  // Play and get shutdown sound duration from chrome in |sound_duration|. And
  // start real shutdown after a delay of |duration|.
  base::TimeDelta sound_duration =
      std::min(Shell::Get()->accessibility_controller()->PlayShutdownSound(),
               base::Milliseconds(kMaxShutdownSoundDurationMs));
  duration = std::max(duration, sound_duration);
  real_shutdown_timer_.Start(FROM_HERE, duration, this,
                             &LockStateController::OnRealPowerTimeout);
}

void LockStateController::OnRealPowerTimeout() {
  VLOG(1) << "OnRealPowerTimeout";
  DCHECK(shutting_down_);
  DCHECK(shutdown_reason_);
  // Shut down or reboot based on device policy.
  shutdown_controller_->ShutDownOrReboot(*shutdown_reason_);
}

void LockStateController::PreLockAnimation(
    SessionStateAnimator::AnimationSpeed speed,
    bool request_lock_on_completion) {
  VLOG(1) << "PreLockAnimation";
  saved_blur_ = Shell::GetPrimaryRootWindowController()
                    ->wallpaper_widget_controller()
                    ->GetWallpaperBlur();
  Shell::Get()->wallpaper_controller()->UpdateWallpaperBlurForLockState(true);
  auto next_animation_starter = base::BindOnce(
      &LockStateController::PreLockAnimationFinished,
      weak_ptr_factory_.GetWeakPtr(), request_lock_on_completion);
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(std::move(next_animation_starter));

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT, speed);
  animation_sequence->StartAnimation(SessionStateAnimator::SHELF,
                                     SessionStateAnimator::ANIMATION_FADE_OUT,
                                     speed);
  // Hide the screen locker containers so we can raise them later.
  animator_->StartAnimation(SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  AnimateWallpaperAppearanceIfNecessary(speed, animation_sequence);

  animation_sequence->EndSequence();
}

void LockStateController::StartPostLockAnimation() {
  VLOG(1) << "StartPostLockAnimation";
  auto next_animation_starter =
      base::BindOnce(&LockStateController::PostLockAnimationFinished,
                     weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(std::move(next_animation_starter));

  animation_sequence->StartAnimation(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN,
      post_lock_immediate_animation_
          ? SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE
          : SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  // Show the lock screen shelf. This is a no-op if views-based shelf is
  // disabled, since shelf is in NonLockScreenContainersContainer.
  animation_sequence->StartAnimation(
      SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN,
      post_lock_immediate_animation_
          ? SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE
          : SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->EndSequence();
  post_lock_fail_timer_.Start(FROM_HERE, kPostLockFailTimeout, this,
                              &LockStateController::OnPostLockFailTimeout);
}

void LockStateController::StartUnlockAnimationBeforeLockUIDestroyed(
    base::OnceClosure callback) {
  VLOG(1) << "StartUnlockAnimationBeforeLockUIDestroyed";
  animating_unlock_ = true;
  // Hide the lock screen shelf. This is a no-op if views-based shelf is
  // disabled, since shelf is in NonLockScreenContainersContainer.
  animator_->StartAnimation(SessionStateAnimator::SHELF,
                            SessionStateAnimator::ANIMATION_FADE_OUT,
                            SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animator_->StartAnimationWithCallback(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_LIFT,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, std::move(callback));
  animator_->StartAnimation(SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
                            SessionStateAnimator::ANIMATION_COPY_LAYER,
                            SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
}

void LockStateController::StartUnlockAnimationAfterLockUIDestroyed() {
  VLOG(1) << "StartUnlockAnimationAfterLockUIDestroyed";
  auto next_animation_starter = base::BindOnce(
      &LockStateController::UnlockAnimationAfterLockUIDestroyedFinished,
      weak_ptr_factory_.GetWeakPtr());
  SessionStateAnimator::AnimationSequence* animation_sequence =
      animator_->BeginAnimationSequence(std::move(next_animation_starter));

  animation_sequence->StartAnimation(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_DROP,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  animation_sequence->StartAnimation(
      SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN,
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  AnimateWallpaperHidingIfNecessary(
      SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, animation_sequence);
  animation_sequence->EndSequence();
}

void LockStateController::LockAnimationCancelled(bool aborted) {
  VLOG(1) << "LockAnimationCancelled: aborted=" << aborted;
  RestoreUnlockedProperties();
}

void LockStateController::PreLockAnimationFinished(bool request_lock,
                                                   bool aborted) {
  VLOG(1) << "PreLockAnimationFinished: aborted=" << aborted;
  // Aborted in this stage means the locking animation was cancelled by
  // `CancelLockAnimation()`, triggered by releasing a lock button before
  // finishing animation.
  if (aborted)
    return;

  // Don't do anything (including starting the lock-fail timer) if the screen
  // was already locked while the animation was going.
  if (system_is_locked_) {
    DCHECK(!request_lock) << "Got request to lock already-locked system "
                          << "at completion of pre-lock animation";
    return;
  }

  if (request_lock) {
    base::RecordAction(base::UserMetricsAction("Accel_LockScreen_LockButton"));
    Shell::Get()->session_controller()->LockScreen();
  }

  VLOG(1) << "b/228873153 : Starting lock fail timer";
  lock_fail_timer_.Start(FROM_HERE, kLockFailTimeout, this,
                         &LockStateController::OnLockFailTimeout);

  lock_duration_timer_ = std::make_unique<base::ElapsedTimer>();
}

void LockStateController::OnPostLockFailTimeout() {
  VLOG(1) << "OnPostLockFailTimeout";
  PostLockAnimationFinished(true);
}

void LockStateController::PostLockAnimationFinished(bool aborted) {
  VLOG(1) << "PostLockAnimationFinished: aborted=" << aborted;
  if (!animating_lock_)
    return;
  animating_lock_ = false;
  post_lock_immediate_animation_ = false;
  post_lock_fail_timer_.Stop();
  OnLockStateEvent(LockStateObserver::EVENT_LOCK_ANIMATION_FINISHED);
  if (!lock_screen_displayed_callback_.is_null())
    std::move(lock_screen_displayed_callback_).Run();

  CHECK(!views::MenuController::GetActiveInstance());
}

void LockStateController::UnlockAnimationAfterLockUIDestroyedFinished(
    bool aborted) {
  VLOG(1) << "UnlockAnimationAfterLockUIDestroyedFinished: aborted=" << aborted;
  animating_unlock_ = false;
  if (pb_pressed_during_unlock_) {
    Shell::Get()->session_controller()->LockScreen();
    pb_pressed_during_unlock_ = false;
  } else {
    Shell::Get()->wallpaper_controller()->UpdateWallpaperBlurForLockState(
        false);
    RestoreUnlockedProperties();
  }
}

void LockStateController::StoreUnlockedProperties() {
  if (!unlocked_properties_) {
    unlocked_properties_ = std::make_unique<UnlockedStateProperties>();
    unlocked_properties_->wallpaper_is_hidden = animator_->IsWallpaperHidden();
  }
  if (unlocked_properties_->wallpaper_is_hidden) {
    // Hide wallpaper so that it can be animated later.
    animator_->StartAnimation(SessionStateAnimator::WALLPAPER,
                              SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
    animator_->ShowWallpaper();
  }
}

void LockStateController::RestoreUnlockedProperties() {
  if (!unlocked_properties_)
    return;
  if (unlocked_properties_->wallpaper_is_hidden) {
    animator_->HideWallpaper();
    // Restore wallpaper visibility.
    animator_->StartAnimation(SessionStateAnimator::WALLPAPER,
                              SessionStateAnimator::ANIMATION_FADE_IN,
                              SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);
  }
  unlocked_properties_.reset();
}

void LockStateController::AnimateWallpaperAppearanceIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    SessionStateAnimator::AnimationSequence* animation_sequence) {
  if (unlocked_properties_.get() && unlocked_properties_->wallpaper_is_hidden) {
    animation_sequence->StartAnimation(SessionStateAnimator::WALLPAPER,
                                       SessionStateAnimator::ANIMATION_FADE_IN,
                                       speed);
  }
}

void LockStateController::AnimateWallpaperHidingIfNecessary(
    SessionStateAnimator::AnimationSpeed speed,
    SessionStateAnimator::AnimationSequence* animation_sequence) {
  if (unlocked_properties_.get() && unlocked_properties_->wallpaper_is_hidden) {
    animation_sequence->StartAnimation(SessionStateAnimator::WALLPAPER,
                                       SessionStateAnimator::ANIMATION_FADE_OUT,
                                       speed);
  }
}

void LockStateController::OnLockStateEvent(LockStateObserver::EventType event) {
  if (shutting_down_)
    return;

  for (auto& observer : observers_)
    observer.OnLockStateEvent(event);
}

}  // namespace ash
