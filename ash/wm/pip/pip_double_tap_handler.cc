// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_double_tap_handler.h"

#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace {

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

}  // namespace

PipDoubleTapHandler::PipDoubleTapHandler() = default;

PipDoubleTapHandler::~PipDoubleTapHandler() = default;

bool PipDoubleTapHandler::ProcessDoubleTapEvent(const ui::Event& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  WindowState* window_state = WindowState::Get(target->GetToplevelWindow());
  if (!window_state) {
    return false;
  }
  return ProcessDoubleTapEventImpl(event, window_state);
}

bool PipDoubleTapHandler::ProcessDoubleTapEventImpl(const ui::Event& event,
                                                    WindowState* window_state) {
  if (!window_state->IsPip()) {
    return false;
  }

  if (window_util::IsArcPipWindow(window_state->window())) {
    return false;
  }

  if (!IsDoubleTap(event)) {
    return false;
  }

  gfx::Rect bounds = window_state->window()->bounds();

  window_state->set_bounds_changed_by_user(true);

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
