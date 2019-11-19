// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller.h"

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The angle which the screen has to be rotated past before the display will
// rotate to match it (i.e. 45.0f is no stickiness).
const float kDisplayRotationStickyAngleDegrees = 60.0f;

// The minimum acceleration in m/s^2 in a direction required to trigger screen
// rotation. This prevents rapid toggling of rotation when the device is near
// flat and there is very little screen aligned force on it. The value is
// effectively the sine of the rise angle required times the acceleration due
// to gravity, with the current value requiring at least a 25 degree rise.
const float kMinimumAccelerationScreenRotation = 4.2f;

// Return true if auto-rotation is allowed which happens when the device is in a
// physical tablet state.
bool IsAutoRotationAllowed() {
  return Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
}

OrientationLockType GetDisplayNaturalOrientation() {
  if (!display::Display::HasInternalDisplay())
    return OrientationLockType::kLandscape;

  display::ManagedDisplayInfo info =
      Shell::Get()->display_manager()->GetDisplayInfo(
          display::Display::InternalDisplayId());
  gfx::Size size = info.GetSizeInPixelWithPanelOrientation();
  return size.width() > size.height() ? OrientationLockType::kLandscape
                                      : OrientationLockType::kPortrait;
}

OrientationLockType RotationToOrientation(OrientationLockType natural,
                                          display::Display::Rotation rotation) {
  if (natural == OrientationLockType::kLandscape) {
    // To be consistent with Android, the rgotation of the primary portrait
    // on naturally landscape device is 270.
    switch (rotation) {
      case display::Display::ROTATE_0:
        return OrientationLockType::kLandscapePrimary;
      case display::Display::ROTATE_90:
        return OrientationLockType::kPortraitSecondary;
      case display::Display::ROTATE_180:
        return OrientationLockType::kLandscapeSecondary;
      case display::Display::ROTATE_270:
        return OrientationLockType::kPortraitPrimary;
    }
  } else {  // Natural portrait
    switch (rotation) {
      case display::Display::ROTATE_0:
        return OrientationLockType::kPortraitPrimary;
      case display::Display::ROTATE_90:
        return OrientationLockType::kLandscapePrimary;
      case display::Display::ROTATE_180:
        return OrientationLockType::kPortraitSecondary;
      case display::Display::ROTATE_270:
        return OrientationLockType::kLandscapeSecondary;
    }
  }
  NOTREACHED();
  return OrientationLockType::kAny;
}

// Returns the rotation that matches the orientation type.
// Returns ROTATE_0 if the given orientation is ANY, which is used
// to indicate that user didn't lock orientation.
display::Display::Rotation OrientationToRotation(
    OrientationLockType natural,
    OrientationLockType orientation) {
  if (orientation == OrientationLockType::kAny)
    return display::Display::ROTATE_0;

  if (natural == OrientationLockType::kLandscape) {
    // To be consistent with Android, the rotation of the primary portrait
    // on naturally landscape device is 270.
    switch (orientation) {
      case OrientationLockType::kLandscapePrimary:
        return display::Display::ROTATE_0;
      case OrientationLockType::kPortraitPrimary:
        return display::Display::ROTATE_270;
      case OrientationLockType::kLandscapeSecondary:
        return display::Display::ROTATE_180;
      case OrientationLockType::kPortraitSecondary:
        return display::Display::ROTATE_90;
      default:
        break;
    }
  } else {  // Natural portrait
    switch (orientation) {
      case OrientationLockType::kPortraitPrimary:
        return display::Display::ROTATE_0;
      case OrientationLockType::kLandscapePrimary:
        return display::Display::ROTATE_90;
      case OrientationLockType::kPortraitSecondary:
        return display::Display::ROTATE_180;
      case OrientationLockType::kLandscapeSecondary:
        return display::Display::ROTATE_270;
      default:
        break;
    }
  }
  NOTREACHED() << static_cast<int>(orientation);
  return display::Display::ROTATE_0;
}

// Returns the locked orientation that matches the application
// requested orientation, or the application orientation itself
// if it didn't match.
OrientationLockType ResolveOrientationLock(OrientationLockType app_requested,
                                           OrientationLockType lock) {
  if (app_requested == OrientationLockType::kAny ||
      (app_requested == OrientationLockType::kLandscape &&
       (lock == OrientationLockType::kLandscapePrimary ||
        lock == OrientationLockType::kLandscapeSecondary)) ||
      (app_requested == OrientationLockType::kPortrait &&
       (lock == OrientationLockType::kPortraitPrimary ||
        lock == OrientationLockType::kPortraitSecondary))) {
    return lock;
  }
  return app_requested;
}

}  // namespace

bool IsPrimaryOrientation(OrientationLockType type) {
  return type == OrientationLockType::kLandscapePrimary ||
         type == OrientationLockType::kPortraitPrimary;
}

bool IsLandscapeOrientation(OrientationLockType type) {
  return type == OrientationLockType::kLandscape ||
         type == OrientationLockType::kLandscapePrimary ||
         type == OrientationLockType::kLandscapeSecondary;
}

bool IsPortraitOrientation(OrientationLockType type) {
  return type == OrientationLockType::kPortrait ||
         type == OrientationLockType::kPortraitPrimary ||
         type == OrientationLockType::kPortraitSecondary;
}

OrientationLockType GetCurrentScreenOrientation() {
  // ScreenOrientationController might be nullptr during shutdown.
  // TODO(xdai|sammiequon): See if we can reorder so that users of the function
  // |SplitViewController::Get| get shutdown before screen orientation
  // controller.
  if (!Shell::Get()->screen_orientation_controller())
    return OrientationLockType::kAny;
  return Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
}

bool IsCurrentScreenOrientationLandscape() {
  return IsLandscapeOrientation(GetCurrentScreenOrientation());
}

bool IsCurrentScreenOrientationPrimary() {
  return IsPrimaryOrientation(GetCurrentScreenOrientation());
}

std::ostream& operator<<(std::ostream& out, const OrientationLockType& lock) {
  switch (lock) {
    case OrientationLockType::kAny:
      out << "any";
      break;
    case OrientationLockType::kNatural:
      out << "natural";
      break;
    case OrientationLockType::kCurrent:
      out << "current";
      break;
    case OrientationLockType::kPortrait:
      out << "portrait";
      break;
    case OrientationLockType::kLandscape:
      out << "landscape";
      break;
    case OrientationLockType::kPortraitPrimary:
      out << "portrait-primary";
      break;
    case OrientationLockType::kPortraitSecondary:
      out << "portrait-secondary";
      break;
    case OrientationLockType::kLandscapePrimary:
      out << "landscape-primary";
      break;
    case OrientationLockType::kLandscapeSecondary:
      out << "landscape-secondary";
      break;
  }
  return out;
}

ScreenOrientationController::ScreenOrientationController()
    : natural_orientation_(GetDisplayNaturalOrientation()),
      ignore_display_configuration_updates_(false),
      rotation_locked_(false),
      rotation_locked_orientation_(OrientationLockType::kAny),
      user_rotation_(display::Display::ROTATE_0),
      current_rotation_(display::Display::ROTATE_0) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->AddObserver(this);
}

ScreenOrientationController::~ScreenOrientationController() {
  SplitViewController::Get(Shell::GetPrimaryRootWindow())->RemoveObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  AccelerometerReader::GetInstance()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  for (auto& windows : lock_info_map_)
    windows.first->RemoveObserver(this);
}

void ScreenOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenOrientationController::LockOrientationForWindow(
    aura::Window* requesting_window,
    OrientationLockType orientation_lock) {
  if (!requesting_window->HasObserver(this))
    requesting_window->AddObserver(this);
  auto iter = lock_info_map_.find(requesting_window);
  if (iter != lock_info_map_.end()) {
    if (orientation_lock == OrientationLockType::kCurrent) {
      // If the app previously requested an orientation,
      // disable the sensor when that orientation is locked.
      iter->second.lock_completion_behavior =
          LockCompletionBehavior::DisableSensor;
    } else {
      iter->second.orientation_lock = orientation_lock;
      iter->second.lock_completion_behavior = LockCompletionBehavior::None;
    }
  } else {
    lock_info_map_.emplace(requesting_window, LockInfo(orientation_lock));
  }

  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockOrientationForWindow(
    aura::Window* window) {
  lock_info_map_.erase(window);
  window->RemoveObserver(this);
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockAll() {
  SetRotationLockedInternal(false);
  // TODO(oshima): Remove if when current_rotation_ is removed.
  if (user_rotation_ != current_rotation_) {
    SetDisplayRotation(user_rotation_,
                       display::Display::RotationSource::ACCELEROMETER,
                       DisplayConfigurationController::ANIMATION_SYNC);
  }
}

bool ScreenOrientationController::IsUserLockedOrientationPortrait() {
  switch (user_locked_orientation_) {
    case OrientationLockType::kPortraitPrimary:
    case OrientationLockType::kPortraitSecondary:
    case OrientationLockType::kPortrait:
      return true;
    default:
      return false;
  }
}

void ScreenOrientationController::ToggleUserRotationLock() {
  if (!display::Display::HasInternalDisplay())
    return;

  if (user_rotation_locked()) {
    SetLockToOrientation(OrientationLockType::kAny);
  } else {
    display::Display::Rotation current_rotation =
        Shell::Get()
            ->display_manager()
            ->GetDisplayInfo(display::Display::InternalDisplayId())
            .GetActiveRotation();
    SetLockToRotation(current_rotation);
  }
}

void ScreenOrientationController::SetLockToRotation(
    display::Display::Rotation rotation) {
  if (!display::Display::HasInternalDisplay())
    return;

  SetLockToOrientation(RotationToOrientation(natural_orientation_, rotation));
}

OrientationLockType ScreenOrientationController::GetCurrentOrientation() const {
  return RotationToOrientation(natural_orientation_, current_rotation_);
}

void ScreenOrientationController::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnWindowDestroying(aura::Window* window) {
  UnlockOrientationForWindow(window);
}

// Currently contents::WebContents will only be able to lock rotation while
// fullscreen. In this state a user cannot click on the tab strip to change. If
// this becomes supported for non-fullscreen tabs then the following interferes
// with TabDragController. OnWindowVisibilityChanged is called between a mouse
// down and mouse up. The rotation this triggers leads to a coordinate space
// change in the middle of an event. Causes the tab to separate from the tab
// strip.
void ScreenOrientationController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (base::Contains(lock_info_map_, window))
    ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnAccelerometerUpdated(
    scoped_refptr<const AccelerometerUpdate> update) {
  if (!IsAutoRotationAllowed())
    return;

  if (rotation_locked_ && !CanRotateInLockedState())
    return;
  if (!update->has(ACCELEROMETER_SOURCE_SCREEN))
    return;
  // Ignore the reading if it appears unstable. The reading is considered
  // unstable if it deviates too much from gravity
  if (update->IsReadingStable(ACCELEROMETER_SOURCE_SCREEN))
    HandleScreenRotation(update->get(ACCELEROMETER_SOURCE_SCREEN));
}

void ScreenOrientationController::OnDisplayConfigurationChanged() {
  if (ignore_display_configuration_updates_)
    return;
  if (!display::Display::HasInternalDisplay())
    return;
  if (!Shell::Get()->display_manager()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    return;
  }
  // TODO(oshima): remove current_rotation_ and always use the target rotation.
  current_rotation_ =
      Shell::Get()->display_configuration_controller()->GetTargetRotation(
          display::Display::InternalDisplayId());
}

void ScreenOrientationController::OnTabletModeStarted() {
  // Observe window activation only while in UI tablet mode, since this the only
  // mode in which we apply apps' requested orientation locks.
  Shell::Get()->activation_client()->AddObserver(this);

  if (!display::Display::HasInternalDisplay())
    return;
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnTabletModeEnded() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (!display::Display::HasInternalDisplay())
    return;

  if (!IsAutoRotationAllowed()) {
    // Rotation locks should have been cleared already in
    // `OnTabletPhysicalStateChanged()`.
    DCHECK(!rotation_locked());
    DCHECK_EQ(rotation_locked_orientation_, OrientationLockType::kAny);
    return;
  }

  // Auto-rotation is still allowed (since device is still in a physical tablet
  // state). We no-longer apply app's requested orientation locks, so we'll
  // call `ApplyLockForActiveWindow()` to apply the `user_locked_orientation_`
  // if any.
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnTabletPhysicalStateChanged() {
  auto* shell = Shell::Get();

  if (IsAutoRotationAllowed()) {
    AccelerometerReader::GetInstance()->AddObserver(this);
    shell->window_tree_host_manager()->AddObserver(this);

    // Do not exit early, as the internal display can be determined after
    // Maximize Mode has started. (chrome-os-partner:38796) Always start
    // observing.
    if (display::Display::HasInternalDisplay()) {
      current_rotation_ = user_rotation_ =
          shell->display_configuration_controller()->GetTargetRotation(
              display::Display::InternalDisplayId());
    }
    if (!rotation_locked_)
      LoadDisplayRotationProperties();

    if (!display::Display::HasInternalDisplay())
      return;
    ApplyLockForActiveWindow();
  } else {
    AccelerometerReader::GetInstance()->RemoveObserver(this);
    shell->window_tree_host_manager()->RemoveObserver(this);

    if (!display::Display::HasInternalDisplay())
      return;

    UnlockAll();
  }

  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (previous_state == SplitViewController::State::kNoSnap ||
      state == SplitViewController::State::kNoSnap) {
    ApplyLockForActiveWindow();
  }
}

void ScreenOrientationController::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source,
    DisplayConfigurationController::RotationAnimation mode) {
  if (!display::Display::HasInternalDisplay())
    return;
  current_rotation_ = rotation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);

  Shell::Get()->display_configuration_controller()->SetDisplayRotation(
      display::Display::InternalDisplayId(), rotation, source, mode);
}

void ScreenOrientationController::SetRotationLockedInternal(
    bool rotation_locked) {
  if (rotation_locked_ == rotation_locked)
    return;
  rotation_locked_ = rotation_locked;
  if (!rotation_locked_)
    rotation_locked_orientation_ = OrientationLockType::kAny;
}

void ScreenOrientationController::SetLockToOrientation(
    OrientationLockType orientation) {
  user_locked_orientation_ = orientation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  Shell::Get()->display_manager()->RegisterDisplayRotationProperties(
      user_rotation_locked(),
      OrientationToRotation(natural_orientation_, user_locked_orientation_));

  ApplyLockForActiveWindow();
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::LockRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source) {
  SetRotationLockedInternal(true);
  SetDisplayRotation(rotation, source);
}

void ScreenOrientationController::LockRotationToOrientation(
    OrientationLockType lock_orientation) {
  rotation_locked_orientation_ = lock_orientation;
  switch (lock_orientation) {
    case OrientationLockType::kAny:
      SetRotationLockedInternal(false);
      break;
    case OrientationLockType::kLandscape:
    case OrientationLockType::kPortrait:
      LockToRotationMatchingOrientation(lock_orientation);
      break;

    case OrientationLockType::kLandscapePrimary:
    case OrientationLockType::kLandscapeSecondary:
    case OrientationLockType::kPortraitPrimary:
    case OrientationLockType::kPortraitSecondary:
      LockRotation(
          OrientationToRotation(natural_orientation_, lock_orientation),
          display::Display::RotationSource::ACTIVE);

      break;
    case OrientationLockType::kNatural:
      LockRotation(display::Display::ROTATE_0,
                   display::Display::RotationSource::ACTIVE);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ScreenOrientationController::LockToRotationMatchingOrientation(
    OrientationLockType lock_orientation) {
  if (!display::Display::HasInternalDisplay())
    return;

  display::Display::Rotation rotation =
      Shell::Get()
          ->display_manager()
          ->GetDisplayInfo(display::Display::InternalDisplayId())
          .GetActiveRotation();
  if (natural_orientation_ == lock_orientation) {
    if (rotation == display::Display::ROTATE_0 ||
        rotation == display::Display::ROTATE_180) {
      SetRotationLockedInternal(true);
    } else {
      LockRotation(display::Display::ROTATE_0,
                   display::Display::RotationSource::ACTIVE);
    }
  } else {
    if (rotation == display::Display::ROTATE_90 ||
        rotation == display::Display::ROTATE_270) {
      SetRotationLockedInternal(true);
    } else {
      // Rotate to the default rotation of the requested orientation.
      display::Display::Rotation default_rotation =
          natural_orientation_ == OrientationLockType::kLandscape
              ? display::Display::ROTATE_270  // portrait in landscape device.
              : display::Display::ROTATE_90;  // landscape in portrait device.
      LockRotation(default_rotation, display::Display::RotationSource::ACTIVE);
    }
  }
}

void ScreenOrientationController::HandleScreenRotation(
    const AccelerometerReading& lid) {
  gfx::Vector3dF lid_flattened(lid.x, lid.y, 0.0f);
  float lid_flattened_length = lid_flattened.Length();
  // When the lid is close to being flat, don't change rotation as it is too
  // sensitive to slight movements.
  if (lid_flattened_length < kMinimumAccelerationScreenRotation)
    return;

  // The reference vector is the angle of gravity when the device is rotated
  // clockwise by 45 degrees. Computing the angle between this vector and
  // gravity we can easily determine the expected display rotation.
  static constexpr gfx::Vector3dF rotation_reference(-1.0f, 1.0f, 0.0f);

  // Set the down vector to match the expected direction of gravity given the
  // last configured rotation. This is used to enforce a stickiness that the
  // user must overcome to rotate the display and prevents frequent rotations
  // when holding the device near 45 degrees.
  gfx::Vector3dF down(0.0f, 0.0f, 0.0f);
  if (current_rotation_ == display::Display::ROTATE_0)
    down.set_y(1.0f);
  else if (current_rotation_ == display::Display::ROTATE_90)
    down.set_x(1.0f);
  else if (current_rotation_ == display::Display::ROTATE_180)
    down.set_y(-1.0f);
  else
    down.set_x(-1.0f);

  // Don't rotate if the screen has not passed the threshold.
  if (gfx::AngleBetweenVectorsInDegrees(down, lid_flattened) <
      kDisplayRotationStickyAngleDegrees) {
    return;
  }

  float angle = gfx::ClockwiseAngleBetweenVectorsInDegrees(
      rotation_reference, lid_flattened, gfx::Vector3dF(0.0f, 0.0f, 1.0f));

  display::Display::Rotation new_rotation = display::Display::ROTATE_270;
  if (angle < 90.0f)
    new_rotation = display::Display::ROTATE_0;
  else if (angle < 180.0f)
    new_rotation = display::Display::ROTATE_90;
  else if (angle < 270.0f)
    new_rotation = display::Display::ROTATE_180;

  if (new_rotation != current_rotation_ &&
      IsRotationAllowedInLockedState(new_rotation)) {
    SetDisplayRotation(new_rotation,
                       display::Display::RotationSource::ACCELEROMETER);
  }
}

void ScreenOrientationController::LoadDisplayRotationProperties() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (!display_manager->registered_internal_display_rotation_lock())
    return;
  user_locked_orientation_ = RotationToOrientation(
      natural_orientation_,
      display_manager->registered_internal_display_rotation());
}

void ScreenOrientationController::ApplyLockForActiveWindow() {
  bool in_tablet_mode = Shell::Get()->tablet_mode_controller()->InTabletMode();
  if (!in_tablet_mode) {
    if (IsAutoRotationAllowed()) {
      // We ignore windows and app requested orientation locks while the UI is
      // in clamshell mode when the device is physically in a tablet state.
      // Instead we apply the orientation lock requested by the user.
      LockRotationToOrientation(user_locked_orientation_);
    }

    return;
  }

  if (SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InTabletSplitViewMode()) {
    // While split view is enabled, ignore rotation lock set by windows.
    LockRotationToOrientation(user_locked_orientation_);
    return;
  }

  MruWindowTracker::WindowList mru_windows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));

  for (auto* window : mru_windows) {
    if (!window->TargetVisibility())
      continue;

    if (ApplyLockForWindowIfPossible(window))
      return;
  }

  LockRotationToOrientation(user_locked_orientation_);
}

bool ScreenOrientationController::ApplyLockForWindowIfPossible(
    const aura::Window* window) {
  for (auto& pair : lock_info_map_) {
    const aura::Window* lock_window = pair.first;
    LockInfo& lock_info = pair.second;
    if (lock_window->TargetVisibility() && window->Contains(lock_window)) {
      if (lock_info.orientation_lock == OrientationLockType::kCurrent) {
        // If the app requested "current" without previously
        // specifying an orientation, use the current rotation.
        lock_info.orientation_lock =
            RotationToOrientation(natural_orientation_, current_rotation_);
        LockRotationToOrientation(lock_info.orientation_lock);
      } else {
        const auto orientation_lock = ResolveOrientationLock(
            lock_info.orientation_lock, user_locked_orientation_);
        LockRotationToOrientation(orientation_lock);
        if (lock_info.lock_completion_behavior ==
            LockCompletionBehavior::DisableSensor) {
          lock_info.lock_completion_behavior = LockCompletionBehavior::None;
          lock_info.orientation_lock = orientation_lock;
        }
      }
      return true;
    }
  }

  // The default orientation for all chrome browser/apps windows is
  // ANY, so use the user_locked_orientation_;
  if (static_cast<AppType>(window->GetProperty(aura::client::kAppType)) !=
      AppType::NON_APP) {
    LockRotationToOrientation(user_locked_orientation_);
    return true;
  }
  return false;
}

bool ScreenOrientationController::IsRotationAllowedInLockedState(
    display::Display::Rotation rotation) {
  if (!rotation_locked_)
    return true;

  if (!CanRotateInLockedState())
    return false;

  if (natural_orientation_ == rotation_locked_orientation_) {
    return rotation == display::Display::ROTATE_0 ||
           rotation == display::Display::ROTATE_180;
  } else {
    return rotation == display::Display::ROTATE_90 ||
           rotation == display::Display::ROTATE_270;
  }
  return false;
}

bool ScreenOrientationController::CanRotateInLockedState() {
  return rotation_locked_orientation_ == OrientationLockType::kLandscape ||
         rotation_locked_orientation_ == OrientationLockType::kPortrait;
}

void ScreenOrientationController::UpdateNaturalOrientationForTest() {
  natural_orientation_ = GetDisplayNaturalOrientation();
}

}  // namespace ash
