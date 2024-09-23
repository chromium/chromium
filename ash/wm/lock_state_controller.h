// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_STATE_CONTROLLER_H_
#define ASH_WM_LOCK_STATE_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shutdown_reason.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wm/lock_state_observer.h"
#include "ash/wm/session_state_animator.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/image/image.h"

namespace ash {

class ShutdownController;
enum class ShutdownReason;

// Displays onscreen animations and locks or suspends the system in response to
// the power button being pressed or released.
// Lock workflow:
// Entry point:
//  * StartLockAnimation (bool shutdown after lock) - starts lock that can be
//    cancelled.
// Once it completes, PreLockAnimationFinished is called, and system lock is
// requested. Once system locks and lock UI is created, OnLockStateChanged is
// called, and StartPostLockAnimation is called. In PostLockAnimationFinished
// two things happen : EVENT_LOCK_ANIMATION_FINISHED notification is sent (it
// triggers third part of animation within lock UI), and check for continuing to
// shutdown is made.
//
// Unlock workflow:
// WebUI does first part of animation, and calls OnLockScreenHide(callback) that
// triggers StartUnlockAnimationBeforeUIDestroyed(callback). Once callback is
// called at the end of the animation, lock UI is deleted, system unlocks, and
// OnLockStateChanged is called. It leads to
// StartUnlockAnimationAfterLockUIDestroyed.
class ASH_EXPORT LockStateController : public aura::WindowTreeHostObserver,
                                       public SessionObserver {
 public:
  // A bitfield mask including NON_LOCK_SCREEN_CONTAINERS and LAUNCHER, used for
  // pre-lock hiding animation.
  static const int kPreLockContainersMask;

  LockStateController(ShutdownController* shutdown_controller,
                      PrefService* local_state);

  LockStateController(const LockStateController&) = delete;
  LockStateController& operator=(const LockStateController&) = delete;

  ~LockStateController() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(LockStateObserver* observer);
  void RemoveObserver(LockStateObserver* observer);

  // Starts locking (with slow pre-lock animation) that can be cancelled.
  void StartLockAnimation();

  // Starts locking without slow animation.
  void LockWithoutAnimation();

  // Returns true if we have requested system to lock, but haven't received
  // confirmation yet.
  bool LockRequested();

  // Cancels locking and reverts lock animation.
  void CancelLockAnimation();

  // Called when ScreenLocker is ready to close, but not yet destroyed.
  // Can be used to display "hiding" animations on unlock.
  // |callback| will be called when all animations are done.
  void OnLockScreenHide(SessionStateAnimator::AnimationCallback callback);

  // Sets up the callback that should be called once lock animation is finished.
  // Callback is guaranteed to be called once and then discarded.
  void SetLockScreenDisplayedCallback(base::OnceClosure callback);

  // Displays the shutdown animation and requests a system shutdown or system
  // restart depending on the the state of the |RebootOnShutdown| device policy.
  void RequestShutdown(ShutdownReason reason);

  // The difference between this and `RequestShutdown` is that this one starts
  // the shutdown that can be canceled. Note, please use only when necessary and
  // together with `MaybeCancelShutdownAnimation`. E.g., requesting through the
  // physical power button, while pressing the power button with different
  // duration can lead to different shutdown states.
  void RequestCancelableShutdown(ShutdownReason reason);

  // True if the real non-cancelable shutting down started.
  bool ShutdownRequested() const;

  // Reverts the shutdown animation and updates the shutdown state to canceled.
  // Then the shutdown process will not move forward. Returns true if the
  // shutdown is canceled, otherwise false.
  bool MaybeCancelShutdownAnimation();

  // Requests restart with the same animation as `RequestShutdown` and take the
  // informed restore image if forest feature is enabled, restart directly
  // otherwise. `description` is a human-readable string describing the source
  // of request the restart.
  void RequestRestart(power_manager::RequestRestartReason reason,
                      const std::string& description);

  // Requests sign out with the same animation as `RequestShutdown` and take the
  // informed restore image if forest feature is enabled, sign out directly
  // otherwise.
  void RequestSignOut();

  // aura::WindowTreeHostObserver override:
  void OnHostCloseRequested(aura::WindowTreeHost* host) override;

  // SessionObserver overrides:
  void OnChromeTerminating() override;
  void OnLockStateChanged(bool locked) override;

  void set_animator_for_test(SessionStateAnimator* animator) {
    animator_.reset(animator);
  }
  bool animating_lock_for_test() const { return animating_lock_; }

 private:
  friend class LockStateControllerTestApi;

  struct UnlockedStateProperties {
    bool wallpaper_is_hidden;
  };

  // Specifies the requested session state.
  enum class RequestedSessionState {
    kShutdown = 0,
    kCancelableShutdown,
    kRestart,
    kSignOut,
  };

  // Cancels unlock animation.
  void CancelUnlockAnimation();

  // Reverts the pre-lock animation, reports the error.
  void OnLockFailTimeout();

  void PreLockAnimation(SessionStateAnimator::AnimationSpeed speed,
                        bool request_lock_on_completion);
  void StartPostLockAnimation();
  void OnPostLockFailTimeout();
  // This method calls |callback| when animation completes.
  void StartUnlockAnimationBeforeLockUIDestroyed(base::OnceClosure callback);
  void StartUnlockAnimationAfterLockUIDestroyed();

  // These methods are called when corresponding animation completes.
  void LockAnimationCancelled(bool aborted);
  void PreLockAnimationFinished(bool request_lock, bool aborted);
  void PostLockAnimationFinished(bool aborted);
  void UnlockAnimationAfterLockUIDestroyedFinished(bool aborted);

  // Stores properties of UI that have to be temporarily modified while locking.
  void StoreUnlockedProperties();
  void RestoreUnlockedProperties();

  // Fades in wallpaper layer with |speed| if it was hidden in unlocked state.
  void AnimateWallpaperAppearanceIfNecessary(
      SessionStateAnimator::AnimationSpeed speed,
      SessionStateAnimator::AnimationSequence* animation_sequence);

  // Fades out wallpaper layer with |speed| if it was hidden in unlocked state.
  void AnimateWallpaperHidingIfNecessary(
      SessionStateAnimator::AnimationSpeed speed,
      SessionStateAnimator::AnimationSequence* animation_sequence);

  // Passed as a callback to the animation sequence that runs as part of
  // StartUnlockAnimationBeforeLockUIDestroyed. The callback will be invoked
  // after the animations complete, it will then check if the power button was
  // pressed at all during the unlock animation, and if so, immediately revert
  // the animations and notify ScreenLocker that the unlock process is to be
  // aborted.
  void OnUnlockAnimationBeforeLockUIDestroyedFinished();

  // Notifies observers.
  void OnLockStateEvent(LockStateObserver::EventType event);

  // Starts timer for undoable shutdown animation.
  void StartPreShutdownAnimationTimer();

  // Calls `StartSessionStateChangeTimer()` with
  // `RequestedSessionState::kCancelableShutdown` when
  // `cancelable_shutdown_timer_` expires.
  void OnPreShutdownAnimationTimeout();

  // Starts timer for final session state change animation. If
  // `with_animation_time` is true, it will also include time of "fade to white"
  // shutdown animation (NOTE: we are using the same animation for restart and
  // signout as well). If `requested_session_state` is shutdown related,
  // shutdown sound duration will be included in the duration calculation as
  // well.
  void StartSessionStateChangeTimer(
      bool with_animation_time,
      RequestedSessionState requested_session_state);

  // Called by `session_state_change_timer_` to start the
  // `requested_session_state` change.
  void OnSessionStateChangeTimeout(
      RequestedSessionState requested_session_state);

  // Takes a screenshot for the informed restore dialog first and then starts
  // the session state change process. `requested_session_state` indicates the
  // requested session state.
  void SessionStateChangeWithInformedRestore(
      RequestedSessionState requested_session_state);

  // Binds to a callback that will be called by the DLP manager to let us know
  // whether capturing the screenshot should `proceed` or abort due to some
  // restricted contents on the screen. `requested_session_state` indicates the
  // requested session state change.
  void OnDlpRestrictionCheckedAtScreenCapture(
      RequestedSessionState requested_session_state,
      const base::FilePath& file_path,
      bool proceed);

  // Starts the session state change process with the given
  // `requested_session_state`.
  void StartSessionStateChange(RequestedSessionState requested_session_state);

  // Triggers the session state change process when the
  // `take_screenshot_fail_timer_` times out. `requested_session_state`
  // indicates the requested session state change.
  void OnTakeScreenshotFailTimeout(
      RequestedSessionState requested_session_state);

  // Callback invoked once the image is taken. `requested_session_state`
  // indicates the requested session state after the image had been taken.
  // `file_path` indicates the path to save the informed restore image. Note:
  // `gfx::Image` is cheap to pass by value.
  void OnInformedRestoreImageTaken(
      RequestedSessionState requested_session_state,
      const base::FilePath& file_path,
      base::TimeTicks start_time,
      gfx::Image informed_restore_image);

  // Callback invoked when the informed restore image was encoded and saved.
  // `file_path` is the file path to save the informed restore image.
  void OnInformedRestoreImageSaved(base::TimeTicks start_time,
                                   const base::FilePath& file_path);

  // Called when `session_state_change_timer_` times out with `kRestart`
  // requested.
  void DoRestart(power_manager::RequestRestartReason reason,
                 const std::string& description);

  std::unique_ptr<SessionStateAnimator> animator_;

  // Current lock status.
  bool system_is_locked_ = false;

  // True if the real non-cancelable shutting down process started.
  bool shutting_down_ = false;

  // True if the requested cancelable shutdown gets canceled.
  bool shutdown_canceled_ = false;

  // The reason (e.g. user action) for a pending shutdown.
  std::optional<ShutdownReason> shutdown_reason_;

  // Callback bound on restart requested and run when
  // `session_state_change_timer_` times out.
  base::OnceClosure restart_callback_;

  // Indicates whether controller should proceed to (cancellable) shutdown after
  // locking.
  bool shutdown_after_lock_ = false;

  // Indicates that controller displays lock animation.
  bool animating_lock_ = false;

  // Indicates that controller displays unlock animation.
  bool animating_unlock_ = false;

  // Indicates that the power button has been pressed during the unlock
  // animation
  bool pb_pressed_during_unlock_ = false;

  // Indicates whether post lock animation should be immediate.
  bool post_lock_immediate_animation_ = false;

  std::unique_ptr<UnlockedStateProperties> unlocked_properties_;

  // How long has it been since the request to lock the screen?
  std::unique_ptr<base::ElapsedTimer> lock_duration_timer_;

  // Controller used to trigger the actual shutdown.
  raw_ptr<ShutdownController, DanglingUntriaged> shutdown_controller_;

  // Started when we request that the screen be locked. When it fires, we
  // assume that our request got dropped.
  base::OneShotTimer lock_fail_timer_;

  // Started when we call StartPostLockAnimation. When it fires, we assume
  // that our request got dropped.
  base::OneShotTimer post_lock_fail_timer_;

  // Started when a cancelable shutdown requested and the shutdown animation
  // triggered. When it fires, the real non-cancelable shutdown will start.
  base::OneShotTimer cancelable_shutdown_timer_;

  // Started when we display the session state change animation (NOTE, shutdown,
  // restart and signout have the same animation). When it fires, we actually
  // request the session state change. Gives the animation time to complete
  // before Chrome etc are shut down.
  base::OneShotTimer session_state_change_timer_;

  base::OnceClosure lock_screen_displayed_callback_;

  base::OnceCallback<void(bool)> start_unlock_callback_;

  // A new layer that mirrors the wallpaper layer, which will be added to the
  // layer hierarchy and help include the wallpaper into the informed restore
  // screenshot.
  std::unique_ptr<ui::Layer> mirror_wallpaper_layer_;

  // A timer tracks the time duration it takes to take the informed restore
  // image. If this timer timeouts before taking the screenshot completes, the
  // shutdown process will be triggered immediately without the informed restore
  // image. This is done to avoid the shutdown process being blocked too long to
  // be noticed by the users.
  base::OneShotTimer take_screenshot_fail_timer_;

  ScopedSessionObserver scoped_session_observer_;

  // The wallpaper blur before entering lock state. Used to restore the
  // wallpaper blur after exiting lock state.
  float saved_blur_;

  base::ObserverList<LockStateObserver>::Unchecked observers_;

  // To access the pref kLoginShutdownTimestampPrefName
  raw_ptr<PrefService> local_state_;

  // If set, it will be called once the operation on the informed restore image
  // is completed, either it was deleted or saved to the disk.
  base::OnceClosure informed_restore_image_callback_for_test_;

  // Disables the `take_screenshot_fail_timer_` for test, which means the timer
  // will never start if this is set to true.
  bool disable_screenshot_timeout_for_test_ = false;

  base::WeakPtrFactory<LockStateController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_LOCK_STATE_CONTROLLER_H_
