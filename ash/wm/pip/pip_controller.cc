// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The maximum opacity for the `WindowDimmer`.
constexpr float kPipTuckDimMaximumOpacity = 0.5f;

class PipScopedWindowTuckerDelegate : public ScopedWindowTucker::Delegate {
 public:
  explicit PipScopedWindowTuckerDelegate() {}
  PipScopedWindowTuckerDelegate(const PipScopedWindowTuckerDelegate&) = delete;
  PipScopedWindowTuckerDelegate& operator=(
      const PipScopedWindowTuckerDelegate&) = delete;
  ~PipScopedWindowTuckerDelegate() override = default;

  void PaintTuckHandle(gfx::Canvas* canvas, int width, bool left) override {
    // Flip the canvas horizontally for `left` tuck handle.
    if (left) {
      canvas->Translate(gfx::Vector2d(width, 0));
      canvas->Scale(-1, 1);
    }

    const gfx::ImageSkia& tuck_icon = gfx::CreateVectorIcon(
        kTuckHandleChevronIcon, ScopedWindowTucker::kTuckHandleWidth,
        SK_ColorWHITE);
    canvas->DrawImageInt(tuck_icon, 0, 0);
  }

  int ParentContainerId() const override { return kShellWindowId_PipContainer; }

  void UpdateWindowPosition(aura::Window* window, bool left) override {
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInParent(window);

    gfx::Rect bounds_in_parent = window->bounds();
    int bounds_left;
    if (Shell::Get()->pip_controller()->is_tucked()) {
      if (left) {
        bounds_left =
            -bounds_in_parent.width() + ScopedWindowTucker::kTuckHandleWidth;
      } else {
        bounds_left = work_area.width() - ScopedWindowTucker::kTuckHandleWidth;
      }
    } else {
      if (left) {
        bounds_left = kCollisionWindowWorkAreaInsetsDp;
      } else {
        bounds_left = work_area.width() - window->bounds().width() -
                      kCollisionWindowWorkAreaInsetsDp;
      }
    }

    bounds_in_parent.set_origin(gfx::Point(bounds_left, bounds_in_parent.y()));
    window->SetBounds(bounds_in_parent);
  }

  void UntuckWindow(aura::Window* window) override {
    Shell::Get()->pip_controller()->UntuckWindow();
  }

  void OnAnimateTuckEnded(aura::Window* window) override {}

  gfx::Rect GetTuckHandleBounds(bool left,
                                const gfx::Rect& window_bounds) const override {
    const gfx::Point tuck_handle_origin =
        left ? window_bounds.right_center() -
                   gfx::Vector2d(ScopedWindowTucker::kTuckHandleWidth,
                                 ScopedWindowTucker::kTuckHandleHeight / 2)
             : window_bounds.left_center() -
                   gfx::Vector2d(0, ScopedWindowTucker::kTuckHandleHeight / 2);
    return gfx::Rect(tuck_handle_origin,
                     gfx::Size(ScopedWindowTucker::kTuckHandleWidth,
                               ScopedWindowTucker::kTuckHandleHeight));
  }
};

}  // namespace

PipController::PipController() = default;
PipController::~PipController() = default;

void PipController::SetPipWindow(aura::Window* window) {
  if (!window || pip_window_ == window) {
    return;
  }

  if (pip_window_) {
    // As removing ARC/Lacros PiP is async, a new PiP could be created before
    // the currently one is fully removed.
    UnsetPipWindow(pip_window_);
  }

  pip_window_ = window;
  is_tucked_ = false;
  scoped_window_tucker_.reset();
  dimmer_.reset();
  pip_window_observation_.Reset();
  pip_window_observation_.Observe(window);
}

void PipController::UnsetPipWindow(aura::Window* window) {
  if (!pip_window_ || pip_window_ != window) {
    // This function can be called with one of window state, visibility, or
    // existence changes, all of which are valid.
    return;
  }
  pip_window_observation_.Reset();
  pip_window_ = nullptr;
  scoped_window_tucker_.reset();
  is_tucked_ = false;
  dimmer_.reset();
}

void PipController::UpdatePipBounds() {
  if (!pip_window_) {
    // It's a bit hard for the caller of this function to tell when PiP is
    // really active (v.s. A PiP window just exists), so allow calling this
    // when not appropriate.
    return;
  }
  if (is_tucked_) {
    // If the window is tucked, we do not want to move it to the resting
    // position.
    return;
  }
  WindowState* window_state = WindowState::Get(pip_window_);
  gfx::Rect new_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(window_state);
  wm::ConvertRectFromScreen(pip_window_->GetRootWindow(), &new_bounds);
  if (pip_window_->bounds() != new_bounds) {
    SetBoundsWMEvent event(new_bounds, /*animate=*/true);
    window_state->OnWMEvent(&event);
  }
}

void PipController::TuckWindow(bool left) {
  CHECK(pip_window_);
  SetDimOpacity(kPipTuckDimMaximumOpacity);
  is_tucked_ = true;
  scoped_window_tucker_ = std::make_unique<ScopedWindowTucker>(
      std::make_unique<PipScopedWindowTuckerDelegate>(), pip_window_, left);
  scoped_window_tucker_->AnimateTuck();
}

void PipController::OnUntuckAnimationEnded() {
  scoped_window_tucker_.reset();
}

void PipController::UntuckWindow() {
  CHECK(pip_window_);

  // The order here matters: `is_tucked_` must be set to true
  // before `UpdateWindowPosition()` or `AnimateUntuck()` gets
  // the untucked window bounds.
  is_tucked_ = false;

  SetDimOpacity(0.f);

  if (scoped_window_tucker_) {
    scoped_window_tucker_->AnimateUntuck(
        base::BindOnce(&PipController::OnUntuckAnimationEnded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

views::Widget* PipController::GetTuckHandleWidget() {
  CHECK(scoped_window_tucker_);
  return scoped_window_tucker_->tuck_handle_widget();
}

void PipController::SetDimOpacity(float opacity) {
  if (!pip_window_) {
    // This function is invoked during drag move, and PiP can get killed during
    // drag move too.
    return;
  }
  if (opacity == 0.f) {
    if (dimmer_) {
      dimmer_->window()->Hide();
    }
  } else {
    if (!dimmer_) {
      // The dimmer is created when it is first needed. It is not created
      // with `SetPipWindow()` because it is called in
      // `OnPrePipStateChange()` before the window fully enters the PiP state.
      dimmer_ = std::make_unique<WindowDimmer>(pip_window_);
      dimmer_->SetDimOpacity(kPipTuckDimMaximumOpacity);
      dimmer_->window()->layer()->SetIsFastRoundedCorner(true);
      dimmer_->window()->layer()->SetRoundedCornerRadius(
          gfx::RoundedCornersF(chromeos::kPipRoundedCornerRadius));
    }
    dimmer_->SetDimOpacity(opacity);
    dimmer_->window()->Show();
  }
}

void PipController::OnWindowDestroying(aura::Window* window) {
  // Ensure to clean up when PiP is gone especially in unit tests.
  UnsetPipWindow(window);
}

}  // namespace ash
