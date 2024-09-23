// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_
#define ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <unordered_map>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/ash_export.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

ASH_EXPORT chromeos::OrientationType GetCurrentScreenOrientation();
ASH_EXPORT bool IsCurrentScreenOrientationLandscape();
ASH_EXPORT bool IsCurrentScreenOrientationPrimary();

ASH_EXPORT std::ostream& operator<<(std::ostream& out,
                                    const chromeos::OrientationType& lock);

// Implements ChromeOS specific functionality for ScreenOrientationProvider.
class ASH_EXPORT ScreenOrientationController
    : public ::wm::ActivationChangeObserver,
      public aura::WindowObserver,
      public AccelerometerReader::Observer,
      public TabletModeObserver,
      public display::DisplayObserver,
      public display::DisplayManagerObserver {
 public:
  // Observer that reports changes to the state of ScreenOrientationProvider's
  // rotation lock.
  class Observer {
   public:
    // Invoked when rotation is locked or unlocked by a user.
    virtual void OnUserRotationLockChanged() {}

   protected:
    virtual ~Observer() = default;
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

  ScreenOrientationController(const ScreenOrientationController&) = delete;
  ScreenOrientationController& operator=(const ScreenOrientationController&) =
      delete;

  ~ScreenOrientationController() override;

  chromeos::OrientationType natural_orientation() const {
    return natural_orientation_;
  }

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Allows/unallows a window to lock the screen orientation.
  void LockOrientationForWindow(aura::Window* requesting_window,
                                chromeos::OrientationType orientation_lock);

  void UnlockOrientationForWindow(aura::Window* window);

  // Unlock all and set the rotation back to the user specified rotation.
  void UnlockAll();

  // Returns true if the user has locked the orientation to portrait, false if
  // the user has locked the orientation to landscape or not locked the
  // orientation.
  bool IsUserLockedOrientationPortrait();

  // Returns the chromeos::OrientationType that is applied on based on whether a
  // rotation lock was requested for an app window, and whether the current
  // system state allows it to lock the rotation (e.g. being in tablet mode, on
  // the internal display, and splitview is inactive).
  chromeos::OrientationType GetCurrentAppRequestedOrientationLock() const;

  bool ignore_display_configuration_updates() const {
    return ignore_display_configuration_updates_;
  }

  // True if |rotation_lock_| has been set and accelerometer updates should not
  // rotate the display.
  bool rotation_locked() const { return rotation_locked_; }

  bool user_rotation_locked() const {
    return user_locked_orientation_ != chromeos::OrientationType::kAny;
  }

  // Trun on/off the user rotation lock. When turned on, it will lock
  // the orientation to the current orientation.
  // |user_rotation_locked()| method returns the current state of the
  // user rotation lock.
  void ToggleUserRotationLock();

  // Set locked to the given |rotation| and save it.
  void SetLockToRotation(display::Display::Rotation rotation);

  // Gets current screen orientation type.
  chromeos::OrientationType GetCurrentOrientation() const;

  // Returns true if auto-rotation is allowed. It happens when the device is in
  // a physical tablet state or kSupportsClamshellAutoRotation is set.
  bool IsAutoRotationAllowed() const;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // AccelerometerReader::Observer:
  void OnECLidAngleDriverStatusChanged(bool is_supported) override {}
  void OnAccelerometerUpdated(const AccelerometerUpdate& update) override;

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // display::DisplayManagerObserver:
  void OnWillProcessDisplayChanges() override;
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

 private:
  friend class ScreenOrientationControllerTestApi;
  class WindowStateChangeNotifier;

  struct LockInfo {
    LockInfo(chromeos::OrientationType lock, aura::Window* root)
        : orientation_lock(lock), root_window(root) {}
    chromeos::OrientationType orientation_lock =
        chromeos::OrientationType::kAny;
    // Tracks the requesting window's root window and is updated whenever it
    // changes.
    raw_ptr<aura::Window> root_window = nullptr;
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

  // Gets the target rotation for the device's internal display from the
  // `DisplayConfigurationController`.
  display::Display::Rotation GetInternalDisplayTargetRotation() const;

  void SetRotationLockedInternal(bool rotation_locked);

  // A helper method that set locked to the given |orientation| and save it.
  void SetLockToOrientation(chromeos::OrientationType orientation);

  // Sets the display rotation to |rotation|. Future accelerometer updates
  // should not be used to change the rotation. SetRotationLocked(false) removes
  // the rotation lock.
  void LockRotation(display::Display::Rotation rotation,
                    display::Display::RotationSource source);

  // Sets the display rotation based on |lock_orientation|. Future accelerometer
  // updates should not be used to change the rotation. SetRotationLocked(false)
  // removes the rotation lock.
  void LockRotationToOrientation(chromeos::OrientationType lock_orientation);

  // For orientations that do not specify primary or secondary, locks to the
  // current rotation if it matches |lock_orientation|. Otherwise locks to a
  // matching rotation.
  void LockToRotationMatchingOrientation(
      chromeos::OrientationType lock_orientation);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const AccelerometerReading& lid);

  // Checks DisplayManager for registered rotation lock, and rotation,
  // preferences. These are then applied.
  void LoadDisplayRotationProperties();

  // Determines the rotation lock, and orientation, for the top-most window on
  // the internal display, and applies it. If there is none, rotation lock will
  // be removed.
  // TODO(oshima|afakhry): This behavior needs to be revised when Android
  // implements multi-display support.
  void ApplyLockForTopMostWindowOnInternalDisplay();

  // If there is a rotation lock that can be applied to window, applies it and
  // returns true. Otherwise returns false.
  bool ApplyLockForWindowIfPossible(const aura::Window* window);

  // Both |chromeos::OrientationType::kLandscape| and
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
  chromeos::OrientationType natural_orientation_;

  // True when changes being applied cause OnDisplayConfigurationChanged() to be
  // called, and for which these changes should be ignored.
  bool ignore_display_configuration_updates_;

  // When true then accelerometer updates should not rotate the display.
  bool rotation_locked_;

  // True while the displays are being updated by the display manager, so that
  // we don't set the display rotation while this operation is in progress.
  bool suspend_orientation_lock_refreshes_ = false;

  // True if there was a request to refresh the orientation lock while the
  // display manager is in the process of updating the displays. When the
  // display manager is done, we check this value to see if we need to refresh
  // the orientation lock.
  bool is_orientation_lock_refresh_pending_ = false;

  // The orientation to which the current |rotation_locked_| was applied.
  chromeos::OrientationType rotation_locked_orientation_;

  // The rotation of the display set by the user. This rotation will be
  // restored upon exiting tablet mode.
  display::Display::Rotation user_rotation_;

  // The orientation of the device locked by the user.
  chromeos::OrientationType user_locked_orientation_ =
      chromeos::OrientationType::kAny;

  // The currently applied orientation lock that was requested by an app if any.
  std::optional<chromeos::OrientationType>
      current_app_requested_orientation_lock_ = std::nullopt;

  // Rotation Lock observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // Tracks all windows that have requested a lock, as well as the requested
  // orientation.
  std::unordered_map<aura::Window*, LockInfo> lock_info_map_;

  // Register for display configuration changes.
  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};

  display::ScopedDisplayObserver display_observer_{this};

  std::unique_ptr<WindowStateChangeNotifier> window_state_change_notifier_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_H_
