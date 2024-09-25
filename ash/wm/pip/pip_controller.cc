// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_controller.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The maximum opacity for the `WindowDimmer`.
constexpr float kPipTuckDimMaximumOpacity = 0.5f;
constexpr base::TimeDelta kSizeChangeAnimationDuration =
    base::Milliseconds(300);
constexpr float kDefaultPipSizeFromWorkAreaPercent = 0.20f;

// Calculates the new size of a given buffer size while preserving the aspect
// ratio of the PiP window.
gfx::Size PreserveAspectRatio(aura::Window* pip_window,
                              const gfx::Size& buffer_size) {
  gfx::Size max_size = pip_window->delegate()->GetMaximumSize();
  gfx::Size min_size = pip_window->delegate()->GetMinimumSize();
  gfx::Size buffer_size_clone = buffer_size;

  gfx::SizeF* aspect_ratio_size =
      pip_window->GetProperty(aura::client::kAspectRatio);

  buffer_size_clone.SetToMin(max_size);
  buffer_size_clone.SetToMax(min_size);

  float aspect_ratio = aspect_ratio_size->width() / aspect_ratio_size->height();
  gfx::Rect window_rect(pip_window->bounds().origin(), buffer_size_clone);

  gfx::SizeRectToAspectRatio(gfx::ResizeEdge::kBottomRight, aspect_ratio,
                             min_size, max_size, &window_rect);

  return window_rect.size();
}

gfx::Size GetMaxSize(WindowState* pip_window_state) {
  // Calculates the max size of the PiP window preserving the aspect ratio.
  gfx::Size max_size = pip_window_state->window()->delegate()->GetMaximumSize();
  return PreserveAspectRatio(pip_window_state->window(), max_size);
}

gfx::Size GetDefaultSize(WindowState* pip_window_state) {
  gfx::Size work_area_size =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(pip_window_state->window()->GetRootWindow())
          .work_area_size();

  gfx::Size window_size = gfx::ScaleToRoundedSize(
      work_area_size, kDefaultPipSizeFromWorkAreaPercent);

  return PreserveAspectRatio(pip_window_state->window(), window_size);
}

bool IsAtMaxSize(int width, int height, const gfx::Size& max_size) {
  return width == max_size.width() && height == max_size.height();
}

bool IsDoubleTap(const ui::Event& event) {
  return (event.IsMouseEvent() && event.flags() & ui::EF_IS_DOUBLE_CLICK) ||
         (event.IsGestureEvent() &&
          event.AsGestureEvent()->details().tap_count() == 2);
}

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

bool PipController::CanResizePip() {
  if (!pip_window_) {
    return false;
  }
  gfx::Size max_size = pip_window_->delegate()->GetMaximumSize();
  gfx::Size min_size = pip_window_->delegate()->GetMinimumSize();
  return !max_size.IsEmpty() && !min_size.IsEmpty() &&
         max_size.width() > min_size.width() &&
         max_size.height() > min_size.height();
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
  if (!ash::PipPositioner::HasSnapFraction(window_state) &&
      IsArcWindow(pip_window_)) {
    // Prevent PiP bounds from being updated between window state change into
    // PiP and initial bounds change for PiP. This is only needed for ARC
    // because chrome PiP becomes visible only after both window state and
    // initial bounds are set properly while in the case of ARC a normal visible
    // window can trainsition to PiP. Also, in fact, this check is only valid
    // for ARC PiP as the first timing snap fraction is set is different between
    // chrome PiP and ARC PiP.
    return;
  }
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

bool PipController::HandleDoubleTap(const ui::Event& event) {
  return pip_size_switch_handler_.ProcessDoubleTapEvent(event);
}

bool PipController::HandleKeyboardShortcut() {
  return pip_size_switch_handler_.ProcessShortcutEvent(pip_window_);
}

bool PipController::PipSizeSwitchHandler::ProcessDoubleTapEvent(
    const ui::Event& event) {
  if (!IsDoubleTap(event)) {
    return false;
  }

  aura::Window* target = static_cast<aura::Window*>(event.target());
  WindowState* window_state = WindowState::Get(target->GetToplevelWindow());

  if (!window_state) {
    return false;
  }

  if (window_util::IsArcPipWindow(window_state->window())) {
    return false;
  }

  return ResizePip(window_state);
}

bool PipController::PipSizeSwitchHandler::ProcessShortcutEvent(aura::Window* pip_window) {
  WindowState* window_state = WindowState::Get(pip_window);
  if (!window_state) {
    return false;
  }
  return ResizePip(window_state);
}

bool PipController::PipSizeSwitchHandler::ResizePip(WindowState* window_state) {
  if (!window_state) {
    return false;
  }

  if (!window_state->IsPip()) {
    return false;
  }

  if (!Shell::Get()->pip_controller()->CanResizePip()) {
    return false;
  }

  gfx::Rect bounds = window_state->window()->bounds();

  window_state->SetBoundsChangedByUser(true);

  gfx::Size calculated_max_size = GetMaxSize(window_state);
  // If the window is not at max size, we will expand it.
  if (IsAtMaxSize(bounds.width(), bounds.height(), calculated_max_size)) {
    // If the PiP window is max from the result of a drag resize (without ever
    // double tapping) then go back to default.
    if (prev_bounds_.IsEmpty()) {
      bounds = gfx::Rect(bounds.origin(), GetDefaultSize(window_state));
    } else {
      bounds = prev_bounds_;
    }
  } else {  // Is not max size.
    // Save the bounds of the window prior to expanding.
    prev_bounds_ = bounds;
    bounds.set_size(calculated_max_size);
  }

  // Note that WindowState will place the position of the PiP window in the
  // correct location.
  SetBoundsWMEvent bounds_event(bounds, /*animate=*/true,
                                kSizeChangeAnimationDuration);
  window_state->OnWMEvent(&bounds_event);
  return true;
}

}  // namespace ash
