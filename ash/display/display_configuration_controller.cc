// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_controller.h"

#include <memory>

#include "ash/display/display_animator.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

namespace {

// Specifies how long the display change should have been disabled
// after each display change operations.
// |kCycleDisplayThrottleTimeoutMs| is set to be longer to avoid
// changing the settings while the system is still configurating
// displays. It will be overriden by |kAfterDisplayChangeThrottleTimeoutMs|
// when the display change happens, so the actual timeout is much shorter.
const int64_t kAfterDisplayChangeThrottleTimeoutMs = 500;
const int64_t kCycleDisplayThrottleTimeoutMs = 4000;
const int64_t kSetPrimaryDisplayThrottleTimeoutMs = 500;

bool g_disable_animator_for_test = false;

display::DisplayPositionInUnifiedMatrix GetUnifiedModeShelfCellPosition() {
  const ShelfAlignment alignment =
      Shell::GetPrimaryRootWindowController()->shelf()->alignment();
  switch (alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return display::DisplayPositionInUnifiedMatrix::kBottomLeft;

    case ShelfAlignment::kLeft:
      return display::DisplayPositionInUnifiedMatrix::kTopLeft;

    case ShelfAlignment::kRight:
      return display::DisplayPositionInUnifiedMatrix::kTopRight;
  }

  NOTREACHED();
}

}  // namespace

class DisplayConfigurationController::DisplayChangeLimiter {
 public:
  DisplayChangeLimiter() : throttle_timeout_(base::Time::Now()) {}

  DisplayChangeLimiter(const DisplayChangeLimiter&) = delete;
  DisplayChangeLimiter& operator=(const DisplayChangeLimiter&) = delete;

  void SetThrottleTimeout(int64_t throttle_ms) {
    throttle_timeout_ = base::Time::Now() + base::Milliseconds(throttle_ms);
  }

  bool IsThrottled() const { return base::Time::Now() < throttle_timeout_; }

 private:
  base::Time throttle_timeout_;
};

// static
void DisplayConfigurationController::DisableAnimatorForTest() {
  g_disable_animator_for_test = true;
}

DisplayConfigurationController::DisplayConfigurationController(
    display::DisplayManager* display_manager,
    WindowTreeHostManager* window_tree_host_manager)
    : display_manager_(display_manager),
      window_tree_host_manager_(window_tree_host_manager) {
  display_manager_->AddDisplayManagerObserver(this);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    limiter_ = std::make_unique<DisplayChangeLimiter>();
  }
  if (!g_disable_animator_for_test)
    display_animator_ = std::make_unique<DisplayAnimator>();
}

DisplayConfigurationController::~DisplayConfigurationController() {
  display_manager_->RemoveDisplayManagerObserver(this);
}

void DisplayConfigurationController::SetDisplayLayout(
    std::unique_ptr<display::DisplayLayout> layout) {
  if (display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::BindOnce(&DisplayConfigurationController::SetDisplayLayoutImpl,
                       weak_ptr_factory_.GetWeakPtr(), std::move(layout)));
  } else {
    SetDisplayLayoutImpl(std::move(layout));
  }
}

void DisplayConfigurationController::SetUnifiedDesktopLayoutMatrix(
    const display::UnifiedDesktopLayoutMatrix& matrix) {
  DCHECK(display_manager_->IsInUnifiedMode());

  if (display_animator_) {
    display_animator_->StartFadeOutAnimation(base::BindOnce(
        &DisplayConfigurationController::SetUnifiedDesktopLayoutMatrixImpl,
        weak_ptr_factory_.GetWeakPtr(), matrix));
  } else {
    SetUnifiedDesktopLayoutMatrixImpl(matrix);
  }
}

void DisplayConfigurationController::SetMirrorMode(bool mirror, bool throttle) {
  if (display_manager_->num_connected_displays() <= 1 ||
      display_manager_->IsInMirrorMode() == mirror ||
      (throttle && IsLimited())) {
    return;
  }
  SetThrottleTimeout(kCycleDisplayThrottleTimeoutMs);
  if (display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::BindOnce(&DisplayConfigurationController::SetMirrorModeImpl,
                       weak_ptr_factory_.GetWeakPtr(), mirror));
  } else {
    SetMirrorModeImpl(mirror);
  }
}

void DisplayConfigurationController::SetDisplayRotation(
    int64_t display_id,
    display::Display::Rotation rotation,
    display::Display::RotationSource source,
    DisplayConfigurationController::RotationAnimation mode) {
  // No need to apply animation if the wallpaper isn't set yet during startup.
  if (display_manager_->IsDisplayIdValid(display_id) &&
      Shell::Get()->wallpaper_controller()->is_wallpaper_set()) {
    if (GetTargetRotation(display_id) == rotation)
      return;
    if (display_animator_) {
      ScreenRotationAnimator* screen_rotation_animator =
          GetScreenRotationAnimatorForDisplay(display_id);
      screen_rotation_animator->Rotate(rotation, source, mode);
      return;
    }
  }
  // Invalid |display_id| or animator is disabled; call
  // DisplayManager::SetDisplayRotation directly.
  display_manager_->SetDisplayRotation(display_id, rotation, source);
}

display::Display::Rotation DisplayConfigurationController::GetTargetRotation(
    int64_t display_id) {
  // The display for `display_id` may exist but there may be no root window for
  // it, such as in the case of Unified Display. Query for the target rotation
  // only if the root window exists.
  if (!display_manager_->IsDisplayIdValid(display_id) ||
      !Shell::GetRootWindowForDisplayId(display_id)) {
    return display::Display::ROTATE_0;
  }

  ScreenRotationAnimator* animator =
      GetScreenRotationAnimatorForDisplay(display_id);
  if (animator->IsRotating())
    return animator->GetTargetRotation();

  return display_manager_->GetDisplayInfo(display_id).GetActiveRotation();
}

void DisplayConfigurationController::SetPrimaryDisplayId(int64_t display_id,
                                                         bool throttle) {
  if (display_manager_->GetNumDisplays() <= 1 || (IsLimited() && throttle))
    return;

  SetThrottleTimeout(kSetPrimaryDisplayThrottleTimeoutMs);
  if (display_animator_) {
    display_animator_->StartFadeOutAnimation(
        base::BindOnce(&DisplayConfigurationController::SetPrimaryDisplayIdImpl,
                       weak_ptr_factory_.GetWeakPtr(), display_id));
  } else {
    SetPrimaryDisplayIdImpl(display_id);
  }
}

display::Display
DisplayConfigurationController::GetPrimaryMirroringDisplayForUnifiedDesktop()
    const {
  DCHECK(display_manager_->IsInUnifiedMode());

  return display_manager_->GetMirroringDisplayForUnifiedDesktop(
      GetUnifiedModeShelfCellPosition());
}

void DisplayConfigurationController::OnDidApplyDisplayChanges() {
  // TODO(oshima): Stop all animations.
  SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);
}

// Protected

void DisplayConfigurationController::SetAnimatorForTest(bool enable) {
  if (display_animator_ && !enable)
    display_animator_.reset();
  else if (!display_animator_ && enable)
    display_animator_ = std::make_unique<DisplayAnimator>();
}

// Private

void DisplayConfigurationController::SetThrottleTimeout(int64_t throttle_ms) {
  if (limiter_)
    limiter_->SetThrottleTimeout(throttle_ms);
}

bool DisplayConfigurationController::IsLimited() {
  return limiter_ && limiter_->IsThrottled();
}

void DisplayConfigurationController::SetDisplayLayoutImpl(
    std::unique_ptr<display::DisplayLayout> layout) {
  display_manager_->SetLayoutForCurrentDisplays(std::move(layout));
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

void DisplayConfigurationController::SetMirrorModeImpl(bool mirror) {
  display_manager_->SetMirrorMode(
      mirror ? display::MirrorMode::kNormal : display::MirrorMode::kOff,
      std::nullopt);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

void DisplayConfigurationController::SetPrimaryDisplayIdImpl(
    int64_t display_id) {
  window_tree_host_manager_->SetPrimaryDisplayId(display_id);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

void DisplayConfigurationController::SetUnifiedDesktopLayoutMatrixImpl(
    const display::UnifiedDesktopLayoutMatrix& matrix) {
  display_manager_->SetUnifiedDesktopMatrix(matrix);
  if (display_animator_)
    display_animator_->StartFadeInAnimation();
}

ScreenRotationAnimator*
DisplayConfigurationController::GetScreenRotationAnimatorForDisplay(
    int64_t display_id) {
  auto* root_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  CHECK(root_controller);
  auto* animator = root_controller->GetScreenRotationAnimator();
  CHECK(animator);
  return animator;
}

}  // namespace ash
