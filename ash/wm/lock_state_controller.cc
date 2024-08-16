// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/lock_state_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/cancel_mode.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/shutdown_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/utility/occlusion_tracker_pauser.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/session_state_animator_impl.h"
#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/window_restore_metrics.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
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

// Amount of time to wait after starting to take the informed restore
// screenshot. The task will be stopped if it takes longer than this time
// duration.
constexpr base::TimeDelta kTakeScreenshotFailTimeout = base::Milliseconds(800);

// Records the given `duration` to the given `pref_name` so it can be recorded
// as an UMA metric on the next startup.
void SaveInformedRestoreScreenshotDuration(PrefService* local_state,
                                           const std::string& pref_name,
                                           base::TimeDelta duration) {
  if (!local_state) {
    return;
  }

  local_state->SetTimeDelta(pref_name, duration);
}

// Encodes and saves the given `image` to `file_path`.
void EncodeAndSaveImage(const base::FilePath& file_path, gfx::Image image) {
  CHECK(!base::CurrentUIThread::IsSet());
  if (image.IsEmpty()) {
    base::DeleteFile(file_path);
    return;
  }

  // The width of the resized informed restore image will be fixed and then the
  // height of it will be calculated based on the aspect ratio of the original
  // informed restore image. The resized informed restore image will be saved to
  // disk, decoded and shown with this size directly inside the informed restore
  // dialog later as well.
  const float aspect_ratio = static_cast<float>(image.Height()) / image.Width();
  const int resized_image_height =
      aspect_ratio * informed_restore::kPreviewContainerWidth;
  const auto resized_image = gfx::ResizedImage(
      image, gfx::Size(informed_restore::kPreviewContainerWidth,
                       resized_image_height));
  auto png_bytes = resized_image.As1xPNGBytes();
  auto raw_data = base::make_span(png_bytes->data(), png_bytes->size());
  if (!base::WriteFile(file_path, raw_data)) {
    LOG(ERROR) << "Failed to write informed restore image to "
               << file_path.MaybeAsASCII();
  }
}

// If the given `for_test_callback` is valid, `callback` will be modified
// to be a new callback that runs the original `callback` and then runs
// `for_test_callback` after the former finishes.
// `base::BindPostTask()` is used to guarantee that when `for_test_callback`
// is invoked, it runs on the same thread of the call site (even if `callback`
// is posted to run on a different thread).
// Note that `for_test_callback` will be empty after this function returns.
template <typename Callback>
void MaybeAppendTestCallback(Callback& callback,
                             base::OnceClosure& for_test_callback) {
  if (for_test_callback) {
    callback = std::move(callback).Then(
        base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(for_test_callback)));
  }
}

// Deletes any existing informed restore image if we should change the session
// state without taking the screenshot, then no stale screenshot will be shown
// after the session state changes.
void DeleteInformedRestoreImage(base::OnceClosure& for_test_callback,
                                const base::FilePath& file_path) {
  auto delete_image_cb =
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path);
  MaybeAppendTestCallback(delete_image_cb, for_test_callback);
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::HIGHEST,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             std::move(delete_image_cb));
}

// TODO(minch): Check whether the screenshot should be taken in kiosk mode.
// Returns true if the informed restore screenshot should be taken on session
// state changes.
bool ShouldTakeInformedRestoreScreenshot() {
  auto* shell = Shell::Get();
  // Do not take the informed restore screenshot if it is in overview mode, lock
  // screen, home launcher or pinned mode.
  if (shell->overview_controller()->InOverviewSession()) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedInOverview);
    return false;
  }
  auto* session_controller = shell->session_controller();
  if (session_controller->IsScreenLocked()) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedInLockScreen);
    return false;
  }
  if (session_controller->IsUserGuest() ||
      session_controller->IsUserPublicAccount()) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedInGuestOrPublicUserSession);
    return false;
  }
  if (shell->app_list_controller()->IsHomeScreenVisible()) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedInHomeLauncher);
    return false;
  }
  if (shell->screen_pinning_controller()->IsPinned()) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedInPinnedMode);
    return false;
  }

  bool has_regular_unminimized_window = false;
  for (aura::Window* window :
       shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    const bool is_non_regular_profile_window =
        !shell->saved_desk_delegate()->IsWindowPersistable(window);
    const bool is_minimized = WindowState::Get(window)->IsMinimized();
    // Do not take the screenshot if there is an incognito ash browser window or
    // a lacros window with the non-regular profile.
    if (!is_minimized && is_non_regular_profile_window) {
      RecordScreenshotOnShutdownStatus(
          ScreenshotOnShutdownStatus::kFailedWithIncognito);
      return false;
    }
    has_regular_unminimized_window |=
        !is_non_regular_profile_window && !is_minimized;
  }

  // Take the screenshot if there are unminimized non-incognito windows inside
  // the active desk. Both the float and the always on top window will be
  // counted.
  if (!has_regular_unminimized_window) {
    RecordScreenshotOnShutdownStatus(
        ScreenshotOnShutdownStatus::kFailedWithNoWindows);
  }
  return has_regular_unminimized_window;
}

// Hide the cursor and lock the cursor as well if `lock` is true.
void HideAndMaybeLockCursor(bool lock) {
  Shell* shell = Shell::Get();
  if (auto* cursor_manager = shell->cursor_manager(); cursor_manager) {
    // Hide cursor, but let it reappear if the mouse moves.
    cursor_manager->HideCursor();
    if (lock) {
      cursor_manager->LockCursor();
    }
  }
}

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
  registry->RegisterTimeDeltaPref(
      prefs::kInformedRestoreScreenshotTakenDuration, base::TimeDelta());
  registry->RegisterTimeDeltaPref(
      prefs::kInformedRestoreScreenshotEncodeAndSaveDuration,
      base::TimeDelta());
}

void LockStateController::AddObserver(LockStateObserver* observer) {
  observers_.AddObserver(observer);
}

void LockStateController::RemoveObserver(LockStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LockStateController::StartLockAnimation() {
  if (animating_lock_) {
    return;
  }

  views::MenuController* active_menu_controller =
      views::MenuController::GetActiveInstance();

  if (active_menu_controller) {
    // TODO(http://b/328064674): Please remove the below crash keys once the
    // the crash is fixed. It seems after post lock animation finished there
    // is active menu. This check is moved to the StartLockAnimation, since it
    // seems the check in the post lock animation is too late.

    views::Widget* owner = active_menu_controller->owner();
    SCOPED_CRASH_KEY_STRING256("LockStateController", "StartLockAnimation",
                               owner ? owner->GetName() : "ownerless");
    CHECK(false);
  }

  animating_lock_ = true;
  StoreUnlockedProperties();
  VLOG(1) << "StartLockAnimation";
  PreLockAnimation(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, true);
  DispatchCancelMode();
  OnLockStateEvent(LockStateObserver::EVENT_PRELOCK_ANIMATION_STARTED);
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

void LockStateController::RequestShutdown(ShutdownReason reason) {
  if (shutting_down_) {
    return;
  }

  shutting_down_ = true;
  shutdown_reason_ = reason;
  shutdown_canceled_ = false;

  if (reason == ShutdownReason::LOGIN_SHUT_DOWN_BUTTON) {
    base::Time now_timestamp = base::DefaultClock::GetInstance()->Now();
    local_state_->SetTime(prefs::kLoginShutdownTimestampPrefName,
                          now_timestamp);
  }

  HideAndMaybeLockCursor(/*lock=*/true);
  if (features::IsForestFeatureEnabled()) {
    SessionStateChangeWithInformedRestore(RequestedSessionState::kShutdown);
  } else {
    StartSessionStateChange(RequestedSessionState::kShutdown);
  }
}

void LockStateController::RequestCancelableShutdown(ShutdownReason reason) {
  shutdown_reason_ = reason;
  shutdown_canceled_ = false;

  HideAndMaybeLockCursor(/*lock=*/false);
  if (features::IsForestFeatureEnabled()) {
    SessionStateChangeWithInformedRestore(
        RequestedSessionState::kCancelableShutdown);
  } else {
    StartSessionStateChange(RequestedSessionState::kCancelableShutdown);
  }
}

bool LockStateController::ShutdownRequested() const {
  return shutting_down_;
}

bool LockStateController::MaybeCancelShutdownAnimation() {
  if (ShutdownRequested()) {
    return false;
  }

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
      SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
  shutdown_canceled_ = true;
  if (features::IsForestFeatureEnabled()) {
    // Shutdown maybe canceled before or after image saved. So we need to delete
    // both here and `OnImageSaved`.
    DeleteInformedRestoreImage(informed_restore_image_callback_for_test_,
                               GetInformedRestoreImagePath());
  }
  cancelable_shutdown_timer_.Stop();
  return true;
}

void LockStateController::RequestRestart(
    power_manager::RequestRestartReason reason,
    const std::string& description) {
  if (features::IsForestFeatureEnabled()) {
    HideAndMaybeLockCursor(/*lock=*/false);
    restart_callback_ =
        base::BindOnce(&LockStateController::DoRestart, base::Unretained(this),
                       reason, description);
    SessionStateChangeWithInformedRestore(RequestedSessionState::kRestart);
  } else {
    chromeos::PowerManagerClient::Get()->RequestRestart(reason, description);
  }
}

void LockStateController::RequestSignOut() {
  if (features::IsForestFeatureEnabled()) {
    SessionStateChangeWithInformedRestore(RequestedSessionState::kSignOut);
  } else {
    Shell::Get()->session_controller()->RequestSignOut();
  }
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
    HideAndMaybeLockCursor(/*lock=*/true);
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

void LockStateController::CancelUnlockAnimation() {
  VLOG(1) << "CancelUnlockAnimation";
  pb_pressed_during_unlock_ = true;
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

void LockStateController::StartPreShutdownAnimationTimer() {
  cancelable_shutdown_timer_.Stop();
  cancelable_shutdown_timer_.Start(
      FROM_HERE,
      animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN),
      this, &LockStateController::OnPreShutdownAnimationTimeout);
}

void LockStateController::OnPreShutdownAnimationTimeout() {
  VLOG(1) << "OnPreShutdownAnimationTimeout";
  shutting_down_ = true;

  HideAndMaybeLockCursor(/*lock=*/false);
  StartSessionStateChangeTimer(/*with_animation_time=*/false,
                               RequestedSessionState::kCancelableShutdown);
}

void LockStateController::StartSessionStateChangeTimer(
    bool with_animation_time,
    RequestedSessionState requested_session_state) {
  base::TimeDelta duration = kShutdownRequestDelay;
  if (with_animation_time) {
    duration +=
        animator_->GetDuration(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  }
  // Play and get shutdown sound duration from chrome in |sound_duration|. And
  // start real shutdown after a delay of |duration|.
  if (requested_session_state == RequestedSessionState::kShutdown ||
      requested_session_state == RequestedSessionState::kCancelableShutdown) {
    base::TimeDelta sound_duration =
        std::min(Shell::Get()->accessibility_controller()->PlayShutdownSound(),
                 base::Milliseconds(kMaxShutdownSoundDurationMs));
    duration = std::max(duration, sound_duration);
  }
  session_state_change_timer_.Start(
      FROM_HERE, duration,
      base::BindOnce(&LockStateController::OnSessionStateChangeTimeout,
                     base::Unretained(this), requested_session_state));
}

void LockStateController::OnSessionStateChangeTimeout(
    RequestedSessionState requested_session_state) {
  if (requested_session_state == RequestedSessionState::kShutdown ||
      requested_session_state == RequestedSessionState::kCancelableShutdown) {
    VLOG(1) << "OnSessionStateChangeTimeout with shutdown requested";
  }
  switch (requested_session_state) {
    case RequestedSessionState::kShutdown:
    case RequestedSessionState::kCancelableShutdown:
      DCHECK(shutting_down_);
      DCHECK(shutdown_reason_);
      shutdown_controller_->ShutDownOrReboot(*shutdown_reason_);
      break;
    case RequestedSessionState::kRestart:
      std::move(restart_callback_).Run();
      break;
    case RequestedSessionState::kSignOut:
      Shell::Get()->session_controller()->RequestSignOut();
      break;
  }
}

void LockStateController::SessionStateChangeWithInformedRestore(
    RequestedSessionState requested_session_state) {
  const base::FilePath file_path = GetInformedRestoreImagePath();

  if (!ShouldTakeInformedRestoreScreenshot()) {
    DeleteInformedRestoreImage(informed_restore_image_callback_for_test_,
                               file_path);
    StartSessionStateChange(requested_session_state);
    return;
  }

  // Check if there are any content currently on the screen that are restricted
  // by DLP.
  CaptureModeController::Get()->CheckScreenCaptureDlpRestrictions(
      base::BindOnce(
          &LockStateController::OnDlpRestrictionCheckedAtScreenCapture,
          weak_ptr_factory_.GetWeakPtr(), requested_session_state, file_path));
}

void LockStateController::OnDlpRestrictionCheckedAtScreenCapture(
    RequestedSessionState requested_session_state,
    const base::FilePath& file_path,
    bool proceed) {
  if (!proceed) {
    RecordScreenshotOnShutdownStatus(ScreenshotOnShutdownStatus::kFailedOnDLP);
    StartSessionStateChange(requested_session_state);
    return;
  }

  // TODO(b/319921650): Finalize the expected behavior on multi-display.
  auto* root = Shell::GetRootWindowForNewWindows();

  // Create a new layer that mirrors the painted wallpaper view layer. Adds it
  // to be the bottom-most child of the shutdown screenshot container layer,
  // which is the parent of the active desk container also the container that we
  // are going to take the informed restore screenshot. With this,
  // 1) wallpaper will be included in the screenshot besides the content of the
  //    active desk.
  // 2) screenshot will be taken on the whole desktop instead of the specific
  //    area with windows. This guarantees the windows' relative position inside
  //    the desktop.
  auto* wallpaper_layer = RootWindowController::ForWindow(root)
                              ->wallpaper_widget_controller()
                              ->wallpaper_view()
                              ->layer();
  CHECK(wallpaper_layer && wallpaper_layer->children().empty());
  mirror_wallpaper_layer_ = wallpaper_layer->Mirror();

  auto* informed_restore_screenshot_container =
      root->GetChildById(kShellWindowId_ShutdownScreenshotContainer);
  auto* shutdown_screenshot_layer =
      informed_restore_screenshot_container->layer();
  shutdown_screenshot_layer->Add(mirror_wallpaper_layer_.get());
  shutdown_screenshot_layer->StackAtBottom(mirror_wallpaper_layer_.get());

  if (!disable_screenshot_timeout_for_test_) {
    // Trigger the `take_screenshot_fail_timer_` and start taking the screenshot
    // at the same time. If the timer timeouts before receiving the screenshot,
    // shutdown process will be triggered without the screenshot.
    take_screenshot_fail_timer_.Start(
        FROM_HERE, kTakeScreenshotFailTimeout,
        base::BindOnce(&LockStateController::OnTakeScreenshotFailTimeout,
                       base::Unretained(this), requested_session_state));
  }

  if (auto* workspace_controller = GetWorkspaceController(
          desks_util::GetActiveDeskContainerForRoot(root))) {
    if (BackdropController* backdrop_controller =
            workspace_controller->layout_manager()->backdrop_controller()) {
      backdrop_controller->HideOnTakingInformedRestoreScreenshot();
    }
  }

  // Take the screenshot on the shutdown screenshot container, thus the float
  // and the always on top windows will be included in the screenshot as well.
  ui::GrabWindowSnapshot(
      informed_restore_screenshot_container,
      /*source_rect=*/
      gfx::Rect(informed_restore_screenshot_container->bounds().size()),
      base::BindOnce(&LockStateController::OnInformedRestoreImageTaken,
                     weak_ptr_factory_.GetWeakPtr(), requested_session_state,
                     file_path, base::TimeTicks::Now()));
}

void LockStateController::StartSessionStateChange(
    RequestedSessionState requested_session_state) {
  if (requested_session_state == RequestedSessionState::kCancelableShutdown &&
      shutdown_canceled_) {
    return;
  }

  animator_->StartAnimation(
      SessionStateAnimator::ROOT_CONTAINER,
      SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS,
      SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  if (requested_session_state == RequestedSessionState::kCancelableShutdown) {
    StartPreShutdownAnimationTimer();
  } else {
    StartSessionStateChangeTimer(/*with_animation_time=*/true,
                                 requested_session_state);
  }
}

void LockStateController::OnTakeScreenshotFailTimeout(
    RequestedSessionState requested_session_state) {
  SaveInformedRestoreScreenshotDuration(
      local_state_, prefs::kInformedRestoreScreenshotTakenDuration,
      kTakeScreenshotFailTimeout);
  RecordScreenshotOnShutdownStatus(
      ScreenshotOnShutdownStatus::kFailedOnTakingScreenshotTimeout);
  mirror_wallpaper_layer_.reset();
  DeleteInformedRestoreImage(informed_restore_image_callback_for_test_,
                             GetInformedRestoreImagePath());
  StartSessionStateChange(requested_session_state);
}

void LockStateController::OnInformedRestoreImageTaken(
    RequestedSessionState requested_session_state,
    const base::FilePath& file_path,
    base::TimeTicks start_time,
    gfx::Image informed_restore_image) {
  // Do not proceed if the `take_screenshot_fail_timer_` is stopped, which means
  // taking screenshot process took too long and the shutdown process has been
  // triggered without the informed restore image.
  if (!disable_screenshot_timeout_for_test_ &&
      !take_screenshot_fail_timer_.IsRunning()) {
    return;
  }

  take_screenshot_fail_timer_.Stop();
  SaveInformedRestoreScreenshotDuration(
      local_state_, prefs::kInformedRestoreScreenshotTakenDuration,
      base::TimeTicks::Now() - start_time);

  mirror_wallpaper_layer_.reset();

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::HIGHEST,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&EncodeAndSaveImage, file_path,
                     std::move(informed_restore_image)),
      base::BindOnce(&LockStateController::OnInformedRestoreImageSaved,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     file_path));

  StartSessionStateChange(requested_session_state);
}

void LockStateController::OnInformedRestoreImageSaved(
    base::TimeTicks start_time,
    const base::FilePath& file_path) {
  SaveInformedRestoreScreenshotDuration(
      local_state_, prefs::kInformedRestoreScreenshotEncodeAndSaveDuration,
      // This duration includes the time waiting for the `ThreadPool` to start
      // running the task, also the time that the UI thread waits to get the
      // reply from the `ThreadPool`.
      base::TimeTicks::Now() - start_time);
  RecordScreenshotOnShutdownStatus(ScreenshotOnShutdownStatus::kSucceeded);
  if (shutdown_canceled_) {
    DeleteInformedRestoreImage(informed_restore_image_callback_for_test_,
                               file_path);
  }
  if (informed_restore_image_callback_for_test_) {
    std::move(informed_restore_image_callback_for_test_).Run();
  }
}

void LockStateController::DoRestart(power_manager::RequestRestartReason reason,
                                    const std::string& description) {
  chromeos::PowerManagerClient::Get()->RequestRestart(reason, description);
}

}  // namespace ash
