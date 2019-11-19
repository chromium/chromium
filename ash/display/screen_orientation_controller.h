// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_
#define ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_

#include <unordered_map>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/ash_export.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {

enum class OrientationLockType {
  kAny,
  kNatural,
  kCurrent,
  kPortrait,
  kLandscape,
  kPortraitPrimary,
  kPortraitSecondary,
  kLandscapePrimary,
  kLandscapeSecondary,
};

// Test if the orientation lock type is primary/landscape/portrait.
bool IsPrimaryOrientation(OrientationLockType type);
bool IsLandscapeOrientation(OrientationLockType type);
bool IsPortraitOrientation(OrientationLockType type);

ASH_EXPORT OrientationLockType GetCurrentScreenOrientation();
ASH_EXPORT bool IsCurrentScreenOrientationLandscape();
ASH_EXPORT bool IsCurrentScreenOrientationPrimary();

ASH_EXPORT std::ostream& operator<<(std::ostream& out,
                                    const OrientationLockType& lock);

// Implements ChromeOS specific functionality for ScreenOrientationProvider.
class ASH_EXPORT ScreenOrientationController
    : public ::wm::ActivationChangeObserver,
      public aura::WindowObserver,
      public AccelerometerReader::Observer,
      public WindowTreeHostManager::Observer,
      public TabletModeObserver,
      public SplitViewObserver {
 public:
  // Observer that reports changes to the state of ScreenOrientationProvider's
  // rotation lock.
  class Observer {
   public:
    // Invoked when rotation is locked or unlocked by a user.
    virtual void OnUserRotationLockChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // Controls the behavior after lock is applied to the window (when the window
  // becomes the active window). |DisableSensor| disables the sensor-based
  // rotation and locks to the specific orientation. For example, PORTRAIT may
  // rotate to PORTRAIT_PRIMARY or PORTRAIT_SECONDARY, and will allow rotation
  // between these two. |DisableSensor| disallows the sensor-based rotation by
  // locking the rotation to whichever specific orientation is applied.
  enum class LockCompletionBehavior {
    None,
    DisableSensor,
  };

  ScreenOrientationController();
  ~ScreenOrientationController() override;

  OrientationLockType natural_orientation() const {
    return natural_orientation_;
  }

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Allows/unallows a window to lock the screen orientation.
  void LockOrientationForWindow(aura::Window* requesting_window,
                                OrientationLockType orientation_lock);

  void UnlockOrientationForWindow(aura::Window* window);

  // Unlock all and set the rotation back to the user specified rotation.
  void UnlockAll();

  // Returns true if the user has locked the orientation to portrait, false if
  // the user has locked the orientation to landscape or not locked the
  // orientation.
  bool IsUserLockedOrientationPortrait();

  bool ignore_display_configuration_updates() const {
    return ignore_display_configuration_updates_;
  }

  // True if |rotation_lock_| has been set and accelerometer updates should not
  // rotate the display.
  bool rotation_locked() const { return rotation_locked_; }

  bool user_rotation_locked() const {
    return user_locked_orientation_ != OrientationLockType::kAny;
  }

  // Trun on/off the user rotation lock. When turned on, it will lock
  // the orientation to the current orientation.
  // |user_rotation_locked()| method returns the current state of the
  // user rotation lock.
  void ToggleUserRotationLock();

  // Set locked to the given |rotation| and save it.
  void SetLockToRotation(display::Display::Rotation rotation);

  // Gets current screen orientation type.
  OrientationLockType GetCurrentOrientation() const;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const AccelerometerUpdate> update) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletPhysicalStateChanged() override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

 private:
  friend class ScreenOrientationControllerTestApi;

  struct LockInfo {
    LockInfo() {}
    LockInfo(OrientationLockType lock) : orientation_lock(lock) {}
    OrientationLockType orientation_lock = OrientationLockType::kAny;
    LockCompletionBehavior lock_completion_behavior =
        LockCompletionBehavior::None;
  };

  // Sets the display rotation for the given |source|. The new |rotation| will
  // also become active. Display changed notifications are suppressed for this
  // change.
  void SetDisplayRotation(
      display::Display::Rotation rotation,
      display::Display::RotationSource source,
      DisplayConfigurationController::RotationAnimation mode =
          DisplayConfigurationController::ANIMATION_ASYNC);

  void SetRotationLockedInternal(bool rotation_locked);

  // A helper method that set locked to the given |orientation| and save it.
  void SetLockToOrientation(OrientationLockType orientation);

  // Sets the display rotation to |rotation|. Future accelerometer updates
  // should not be used to change the rotation. SetRotationLocked(false) removes
  // the rotation lock.
  void LockRotation(display::Display::Rotation rotation,
                    display::Display::RotationSource source);

  // Sets the display rotation based on |lock_orientation|. Future accelerometer
  // updates should not be used to change the rotation. SetRotationLocked(false)
  // removes the rotation lock.
  void LockRotationToOrientation(OrientationLockType lock_orientation);

  // For orientations that do not specify primary or secondary, locks to the
  // current rotation if it matches |lock_orientation|. Otherwise locks to a
  // matching rotation.
  void LockToRotationMatchingOrientation(OrientationLockType lock_orientation);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const AccelerometerReading& lid);

  // Checks DisplayManager for registered rotation lock, and rotation,
  // preferences. These are then applied.
  void LoadDisplayRotationProperties();

  // Determines the rotation lock, and orientation, for the currently active
  // window, and applies it. If there is none, rotation lock will be removed.
  void ApplyLockForActiveWindow();

  // If there is a rotation lock that can be applied to window, applies it and
  // returns true. Otherwise returns false.
  bool ApplyLockForWindowIfPossible(const aura::Window* window);

  // Both |OrientationLockType::kLandscape| and
  // |OrientationLock::kPortrait| allow for rotation between the
  // two angles of the same screen orientation
  // (http://www.w3.org/TR/screen-orientation/). Returns true if |rotation| is
  // supported for the current |rotation_locked_orientation_|.
  bool IsRotationAllowedInLockedState(display::Display::Rotation rotation);

  // Certain orientation locks allow for rotation between the two angles of the
  // same screen orientation. Returns true if |rotation_locked_orientation_|
  // allows rotation.
  bool CanRotateInLockedState();

  void UpdateNaturalOrientationForTest();

  // The orientation of the display when at a rotation of 0.
  OrientationLockType natural_orientation_;

  // True when changes being applied cause OnDisplayConfigurationChanged() to be
  // called, and for which these changes should be ignored.
  bool ignore_display_configuration_updates_;

  // When true then accelerometer updates should not rotate the display.
  bool rotation_locked_;

  // The orientation to which the current |rotation_locked_| was applied.
  OrientationLockType rotation_locked_orientation_;

  // The rotation of the display set by the user. This rotation will be
  // restored upon exiting tablet mode.
  display::Display::Rotation user_rotation_;

  // The orientation of the device locked by the user.
  OrientationLockType user_locked_orientation_ = OrientationLockType::kAny;

  // The current rotation set by ScreenOrientationController for the internal
  // display.
  display::Display::Rotation current_rotation_;

  // Rotation Lock observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // Tracks all windows that have requested a lock, as well as the requested
  // orientation.
  std::unordered_map<aura::Window*, LockInfo> lock_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_
