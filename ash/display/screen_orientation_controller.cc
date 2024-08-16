// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller.h"

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector3d_f.h"
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

chromeos::OrientationType GetInternalDisplayNaturalOrientation() {
  if (!display::HasInternalDisplay())
    return chromeos::OrientationType::kLandscape;
  return chromeos::GetDisplayNaturalOrientation(
      Shell::Get()->display_manager()->GetDisplayForId(
          display::Display::InternalDisplayId()));
}

// Returns the locked orientation that matches the application
// requested orientation, or the application orientation itself
// if it didn't match.
chromeos::OrientationType ResolveOrientationLock(
    chromeos::OrientationType app_requested,
    chromeos::OrientationType lock) {
  if (app_requested == chromeos::OrientationType::kAny ||
      (app_requested == chromeos::OrientationType::kLandscape &&
       (lock == chromeos::OrientationType::kLandscapePrimary ||
        lock == chromeos::OrientationType::kLandscapeSecondary)) ||
      (app_requested == chromeos::OrientationType::kPortrait &&
       (lock == chromeos::OrientationType::kPortraitPrimary ||
        lock == chromeos::OrientationType::kPortraitSecondary))) {
    return lock;
  }
  return app_requested;
}

}  // namespace

chromeos::OrientationType GetCurrentScreenOrientation() {
  // ScreenOrientationController might be nullptr during shutdown.
  // TODO(xdai|sammiequon): See if we can reorder so that users of the function
  // |SplitViewController::Get| get shutdown before screen orientation
  // controller.
  if (!Shell::Get()->screen_orientation_controller())
    return chromeos::OrientationType::kAny;
  return Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
}

bool IsCurrentScreenOrientationLandscape() {
  return chromeos::IsLandscapeOrientation(GetCurrentScreenOrientation());
}

bool IsCurrentScreenOrientationPrimary() {
  return chromeos::IsPrimaryOrientation(GetCurrentScreenOrientation());
}

std::ostream& operator<<(std::ostream& out,
                         const chromeos::OrientationType& lock) {
  switch (lock) {
    case chromeos::OrientationType::kAny:
      out << "any";
      break;
    case chromeos::OrientationType::kNatural:
      out << "natural";
      break;
    case chromeos::OrientationType::kCurrent:
      out << "current";
      break;
    case chromeos::OrientationType::kPortrait:
      out << "portrait";
      break;
    case chromeos::OrientationType::kLandscape:
      out << "landscape";
      break;
    case chromeos::OrientationType::kPortraitPrimary:
      out << "portrait-primary";
      break;
    case chromeos::OrientationType::kPortraitSecondary:
      out << "portrait-secondary";
      break;
    case chromeos::OrientationType::kLandscapePrimary:
      out << "landscape-primary";
      break;
    case chromeos::OrientationType::kLandscapeSecondary:
      out << "landscape-secondary";
      break;
  }
  return out;
}

// `WindowStateChangeNotifier` observes an active window state change, and
// call `ApplyLockForTopMostWindowOnInternalDisplay` when the state changes.
class ScreenOrientationController::WindowStateChangeNotifier
    : public wm::ActivationChangeObserver,
      public WindowStateObserver,
      public aura::WindowObserver {
 public:
  explicit WindowStateChangeNotifier(ScreenOrientationController* controller)
      : controller_(controller) {
    activation_observation_.Observe(Shell::Get()->activation_client());
  }
  WindowStateChangeNotifier(const WindowStateChangeNotifier&) = delete;
  WindowStateChangeNotifier& operator=(const WindowStateChangeNotifier&) =
      delete;
  ~WindowStateChangeNotifier() override = default;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    StartObservingWindowIfNeeded(gained_active);
  }

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(
      WindowState* window_state,
      chromeos::WindowStateType old_type) override {
    controller_->ApplyLockForTopMostWindowOnInternalDisplay();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
    window_state_observation_.Reset();
  }

 private:
  void StartObservingWindowIfNeeded(aura::Window* window) {
    if (window == window_observation_.GetSource()) {
      return;
    }

    window_observation_.Reset();
    window_state_observation_.Reset();

    // Orphan window can not have WindowState.
    if (!window || !window->parent()) {
      return;
    }

    if (auto* const window_state = WindowState::Get(window)) {
      window_observation_.Observe(window);
      window_state_observation_.Observe(window_state);
    }
  }

  const raw_ptr<ScreenOrientationController> controller_;

  base::ScopedObservation<WindowState, WindowStateObserver>
      window_state_observation_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_observation_{this};
};

ScreenOrientationController::ScreenOrientationController()
    : natural_orientation_(GetInternalDisplayNaturalOrientation()),
      ignore_display_configuration_updates_(false),
      rotation_locked_(false),
      rotation_locked_orientation_(chromeos::OrientationType::kAny),
      user_rotation_(display::Display::ROTATE_0),
      window_state_change_notifier_(
          std::make_unique<WindowStateChangeNotifier>(this)) {
  display_manager_observation_.Observe(Shell::Get()->display_manager());
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  AccelerometerReader::GetInstance()->AddObserver(this);
}

ScreenOrientationController::~ScreenOrientationController() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  AccelerometerReader::GetInstance()->RemoveObserver(this);
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
    chromeos::OrientationType orientation_lock) {
  if (!requesting_window->HasObserver(this))
    requesting_window->AddObserver(this);
  auto iter = lock_info_map_.find(requesting_window);
  if (iter != lock_info_map_.end()) {
    if (orientation_lock == chromeos::OrientationType::kCurrent) {
      // If the app previously requested an orientation,
      // disable the sensor when that orientation is locked.
      iter->second.lock_completion_behavior =
          LockCompletionBehavior::DisableSensor;
    } else {
      iter->second.orientation_lock = orientation_lock;
      iter->second.lock_completion_behavior = LockCompletionBehavior::None;
    }
  } else {
    lock_info_map_.emplace(
        requesting_window,
        LockInfo(orientation_lock, requesting_window->GetRootWindow()));
  }

  ApplyLockForTopMostWindowOnInternalDisplay();
}

void ScreenOrientationController::UnlockOrientationForWindow(
    aura::Window* window) {
  lock_info_map_.erase(window);
  window->RemoveObserver(this);
  ApplyLockForTopMostWindowOnInternalDisplay();
}

void ScreenOrientationController::UnlockAll() {
  SetRotationLockedInternal(false);
  if (user_rotation_ != GetInternalDisplayTargetRotation()) {
    SetDisplayRotation(user_rotation_,
                       display::Display::RotationSource::ACCELEROMETER,
                       DisplayConfigurationController::ANIMATION_SYNC);
  }
}

bool ScreenOrientationController::IsUserLockedOrientationPortrait() {
  switch (user_locked_orientation_) {
    case chromeos::OrientationType::kPortraitPrimary:
    case chromeos::OrientationType::kPortraitSecondary:
    case chromeos::OrientationType::kPortrait:
      return true;
    default:
      return false;
  }
}

chromeos::OrientationType
ScreenOrientationController::GetCurrentAppRequestedOrientationLock() const {
  return current_app_requested_orientation_lock_.value_or(
      chromeos::OrientationType::kAny);
}

void ScreenOrientationController::ToggleUserRotationLock() {
  if (!display::HasInternalDisplay())
    return;

  if (user_rotation_locked()) {
    SetLockToOrientation(chromeos::OrientationType::kAny);
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
  if (!display::HasInternalDisplay())
    return;

  SetLockToOrientation(RotationToOrientation(natural_orientation_, rotation));
}

chromeos::OrientationType ScreenOrientationController::GetCurrentOrientation()
    const {
  return RotationToOrientation(natural_orientation_,
                               GetInternalDisplayTargetRotation());
}

bool ScreenOrientationController::IsAutoRotationAllowed() const {
  return Shell::Get()
             ->tablet_mode_controller()
             ->is_in_tablet_physical_state() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSupportsClamshellAutoRotation);
}

void ScreenOrientationController::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  ApplyLockForTopMostWindowOnInternalDisplay();
}

void ScreenOrientationController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Window may move to an external display or back to internal (e.g. via
  // shortcut). In this case, we need to undo/redo any orientation lock it
  // applies on the internal display.
  if (!display::HasInternalDisplay())
    return;

  aura::Window* window = params.receiver;
  aura::Window* target = params.target;
  // The target window of the hierarchy change event must be the receiving
  // window itself, or one of its ancestors; we don't care about events
  // happening to descendant windows, since that doesn't indicate a change of
  // |window|'s root.
  for (auto* curr = window; curr != target; curr = curr->parent()) {
    if (!curr)
      return;
  }

  auto iter = lock_info_map_.find(window);
  if (iter != lock_info_map_.end() &&
      iter->second.root_window != window->GetRootWindow()) {
    iter->second.root_window = window->GetRootWindow();
    ApplyLockForTopMostWindowOnInternalDisplay();
  }
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
    ApplyLockForTopMostWindowOnInternalDisplay();
}

void ScreenOrientationController::OnAccelerometerUpdated(
    const AccelerometerUpdate& update) {
  if (!IsAutoRotationAllowed())
    return;

  if (rotation_locked_ && !CanRotateInLockedState())
    return;
  if (!update.has(ACCELEROMETER_SOURCE_SCREEN))
    return;
  // Ignore the reading if it appears unstable. The reading is considered
  // unstable if it deviates too much from gravity
  if (update.IsReadingStable(ACCELEROMETER_SOURCE_SCREEN))
    HandleScreenRotation(update.get(ACCELEROMETER_SOURCE_SCREEN));
}

void ScreenOrientationController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      // Observe window activation only while in UI tablet mode, since this the
      // only mode in which we apply apps' requested orientation locks.
      Shell::Get()->activation_client()->AddObserver(this);

      if (display::HasInternalDisplay()) {
        ApplyLockForTopMostWindowOnInternalDisplay();
      }
      break;
    case display::TabletState::kInClamshellMode:
      Shell::Get()->activation_client()->RemoveObserver(this);
      if (!display::HasInternalDisplay()) {
        break;
      }
      if (!IsAutoRotationAllowed()) {
        // Rotation locks should have been cleared already in
        // `OnTabletPhysicalStateChanged()`.
        DCHECK(!rotation_locked());
        DCHECK_EQ(rotation_locked_orientation_,
                  chromeos::OrientationType::kAny);
        break;
      }

      // Auto-rotation is still allowed (since device is still in a physical
      // tablet state). We no-longer apply app's requested orientation locks, so
      // we'll call `ApplyLockForTopMostWindowOnInternalDisplay()` to apply the
      // `user_locked_orientation_` if any.
      ApplyLockForTopMostWindowOnInternalDisplay();
      break;
  }
}

void ScreenOrientationController::OnTabletPhysicalStateChanged() {
  if (IsAutoRotationAllowed()) {
    // Do not exit early, as the internal display can be determined after
    // Maximize Mode has started. (chrome-os-partner:38796) Always start
    // observing.
    if (display::HasInternalDisplay()) {
      user_rotation_ = GetInternalDisplayTargetRotation();
    }
    if (!rotation_locked_) {
      LoadDisplayRotationProperties();
    }
    if (!display::HasInternalDisplay()) {
      return;
    }
    ApplyLockForTopMostWindowOnInternalDisplay();
  } else {
    if (!display::HasInternalDisplay()) {
      return;
    }

    UnlockAll();
  }

  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::OnWillProcessDisplayChanges() {
  suspend_orientation_lock_refreshes_ = true;
}

void ScreenOrientationController::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  suspend_orientation_lock_refreshes_ = false;
  if (is_orientation_lock_refresh_pending_) {
    // Note: We must set |is_orientation_lock_refresh_pending_| to false first
    // before calling `ApplyLockForTopMostWindowOnInternalDisplay()`, since
    // changing the display's rotation triggers an
    // `OnWillProcessDisplayChanges()` and `OnDidProcessDisplayChanges()` pair,
    // and we don't want to end up here again.
    is_orientation_lock_refresh_pending_ = false;
    ApplyLockForTopMostWindowOnInternalDisplay();
  }
}

void ScreenOrientationController::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source,
    DisplayConfigurationController::RotationAnimation mode) {
  if (!display::HasInternalDisplay())
    return;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);

  Shell::Get()->display_configuration_controller()->SetDisplayRotation(
      display::Display::InternalDisplayId(), rotation, source, mode);
}

display::Display::Rotation
ScreenOrientationController::GetInternalDisplayTargetRotation() const {
  return display::HasInternalDisplay()
             ? Shell::Get()
                   ->display_configuration_controller()
                   ->GetTargetRotation(display::Display::InternalDisplayId())
             : display::Display::ROTATE_0;
}

void ScreenOrientationController::SetRotationLockedInternal(
    bool rotation_locked) {
  if (rotation_locked_ == rotation_locked)
    return;
  rotation_locked_ = rotation_locked;
  if (!rotation_locked_)
    rotation_locked_orientation_ = chromeos::OrientationType::kAny;
}

void ScreenOrientationController::SetLockToOrientation(
    chromeos::OrientationType orientation) {
  user_locked_orientation_ = orientation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  Shell::Get()->display_manager()->RegisterDisplayRotationProperties(
      user_rotation_locked(),
      chromeos::OrientationToRotation(natural_orientation_,
                                      user_locked_orientation_));

  ApplyLockForTopMostWindowOnInternalDisplay();
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
    chromeos::OrientationType lock_orientation) {
  rotation_locked_orientation_ = lock_orientation;
  switch (lock_orientation) {
    case chromeos::OrientationType::kAny:
      SetRotationLockedInternal(false);
      break;
    case chromeos::OrientationType::kLandscape:
    case chromeos::OrientationType::kPortrait:
      LockToRotationMatchingOrientation(lock_orientation);
      break;

    case chromeos::OrientationType::kLandscapePrimary:
    case chromeos::OrientationType::kLandscapeSecondary:
    case chromeos::OrientationType::kPortraitPrimary:
    case chromeos::OrientationType::kPortraitSecondary:
      LockRotation(chromeos::OrientationToRotation(natural_orientation_,
                                                   lock_orientation),
                   display::Display::RotationSource::ACTIVE);

      break;
    case chromeos::OrientationType::kNatural:
      LockRotation(display::Display::ROTATE_0,
                   display::Display::RotationSource::ACTIVE);
      break;
    default:
      NOTREACHED();
  }
}

void ScreenOrientationController::LockToRotationMatchingOrientation(
    chromeos::OrientationType lock_orientation) {
  if (!display::HasInternalDisplay())
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
          natural_orientation_ == chromeos::OrientationType::kLandscape
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
  // internal display's target rotation. This is used to enforce a stickiness
  // that the user must overcome to rotate the display and prevents frequent
  // rotations when holding the device near 45 degrees.
  display::Display::Rotation target_rotation =
      GetInternalDisplayTargetRotation();
  gfx::Vector3dF down(0.0f, 0.0f, 0.0f);
  if (target_rotation == display::Display::ROTATE_0) {
    down.set_y(1.0f);
  } else if (target_rotation == display::Display::ROTATE_90) {
    down.set_x(1.0f);
  } else if (target_rotation == display::Display::ROTATE_180) {
    down.set_y(-1.0f);
  } else {
    down.set_x(-1.0f);
  }

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

  if (new_rotation != target_rotation &&
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

void ScreenOrientationController::ApplyLockForTopMostWindowOnInternalDisplay() {
  if (suspend_orientation_lock_refreshes_) {
    is_orientation_lock_refresh_pending_ = true;
    return;
  }

  current_app_requested_orientation_lock_ = std::nullopt;
  if (!display::HasInternalDisplay())
    return;

  aura::Window* const internal_display_root =
      Shell::GetRootWindowForDisplayId(display::Display::InternalDisplayId());
  if (!internal_display_root) {
    // We might have an internal display, but no root window for it, such as in
    // the case of Unified Display. Also, some tests may not set an internal
    // display.
    // Since rotation lock is applied only on internal displays (see
    // ScreenOrientationController::SetDisplayRotation()), there's no need to
    // continue.
    return;
  }

  if (!display::Screen::GetScreen()->InTabletMode()) {
    if (IsAutoRotationAllowed()) {
      // We ignore windows and app requested orientation locks while the UI is
      // in clamshell mode when the device is physically in a tablet state.
      // Instead we apply the orientation lock requested by the user.
      LockRotationToOrientation(user_locked_orientation_);
    }

    return;
  }

  MruWindowTracker::WindowList mru_windows(
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk));

  for (aura::Window* window : mru_windows) {
    if (window->GetRootWindow() != internal_display_root)
      continue;

    if (!window->TargetVisibility())
      continue;

    auto* const window_state = WindowState::Get(window);
    // If the top visible window is snapped, we ignore any window-requested
    // rotation. Here we shouldn't rely on `SplitViewController::state()` which
    // can be updated in `WindowStateObserver::OnPostWindowStateTypeChange()`
    // because `ApplyLockForTopMostWindowOnInternalDisplay()` is also triggered
    // by the same callback.
    if (window_state->IsSnapped()) {
      break;
    }

    // We don't respect a window-requested rotation when the window is not
    // maximized/fullscreen.
    if (!window_state->IsMaximized() && !window_state->IsFullscreen()) {
      continue;
    }

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
      if (lock_info.orientation_lock == chromeos::OrientationType::kCurrent) {
        // If the app requested "current" without previously
        // specifying an orientation, use the target rotation.
        lock_info.orientation_lock = RotationToOrientation(
            natural_orientation_, GetInternalDisplayTargetRotation());
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
      current_app_requested_orientation_lock_ =
          std::make_optional<chromeos::OrientationType>(
              lock_info.orientation_lock);
      return true;
    }
  }

  // The default orientation for all chrome browser/apps windows is
  // ANY, so use the user_locked_orientation_;
  if (window->GetProperty(chromeos::kAppTypeKey) !=
      chromeos::AppType::NON_APP) {
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
  }
  return rotation == display::Display::ROTATE_90 ||
         rotation == display::Display::ROTATE_270;
}

bool ScreenOrientationController::CanRotateInLockedState() {
  return rotation_locked_orientation_ ==
             chromeos::OrientationType::kLandscape ||
         rotation_locked_orientation_ == chromeos::OrientationType::kPortrait;
}

void ScreenOrientationController::UpdateNaturalOrientationForTest() {
  natural_orientation_ = GetInternalDisplayNaturalOrientation();
}

}  // namespace ash
