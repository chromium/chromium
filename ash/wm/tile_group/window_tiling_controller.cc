// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_tiling_controller.h"

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/containers/adapters.h"
#include "base/numerics/safe_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr float kTilingRatios[] = {
    0, 1.0f / 4, 1.0f / 3, 1.0f / 2, 2.0f / 3, 3.0f / 4, 1,
};
constexpr float kInitialTilingRatio = 1.0f / 2;

bool CanUseWindowBounds(const aura::Window& window,
                        const gfx::Rect& screen_bounds) {
  if (screen_bounds.IsEmpty()) {
    return false;
  }
  if (const auto* delegate = window.delegate()) {
    const gfx::Size min_size = delegate->GetMinimumSize();
    // Empty min_size is ok.
    return min_size.width() <= screen_bounds.width() &&
           min_size.height() <= screen_bounds.height();
  }
  return true;
}

void SetWindowBoundsInScreen(aura::Window* window,
                             const gfx::Rect& screen_bounds,
                             const display::Display& display) {
  if (!CanUseWindowBounds(*window, screen_bounds)) {
    return;
  }

  auto* window_state = WindowState::Get(window);
  if (!window_state->IsNormalStateType()) {
    // TODO(b/308194482): Disable animation, e.g. if this would unmaximize.
    // But having animation may be ok, so need UX input.
    window_state->SetRestoreBoundsInScreen(screen_bounds);
    const WMEvent event(WM_EVENT_NORMAL);
    window_state->OnWMEvent(&event);
    return;
  }
  window->SetBoundsInScreen(screen_bounds, display);
}

// Applies insets to the window.
void ApplyInsets(aura::Window* window,
                 const gfx::Insets& insets,
                 const display::Display& display) {
  gfx::Rect screen_bounds = window->GetBoundsInScreen();
  screen_bounds.Inset(insets);

  SetWindowBoundsInScreen(window, screen_bounds, display);
}

}  // namespace

bool WindowTilingController::CanTilingResize(aura::Window* window) const {
  WindowState* window_state = WindowState::Get(window);
  return window_state &&
         (window_state->IsNormalStateType() || window_state->IsSnapped() ||
          window_state->IsMaximized());
}

void WindowTilingController::OnTilingResizeLeft(aura::Window* window) {
  const gfx::Rect window_bounds = window->GetBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect work_area = display.work_area();

  // "Snap" left if neither left nor right edges are against work area bounds.
  // TODO(b/316962035): Design behavior if window is off screen vertically.
  // Same for the other directions.
  if (window_bounds.x() != work_area.x() &&
      window_bounds.right() != work_area.right()) {
    SetWindowBoundsInScreen(
        window,
        gfx::Rect(work_area.x(), work_area.y(),
                  work_area.width() * kInitialTilingRatio, work_area.height()),
        display);
    return;
  }

  // Resizing left decreases X, so reverse iterate through ratios.
  for (float ratio : base::Reversed(kTilingRatios)) {
    int new_x = work_area.x() + base::ClampRound(ratio * work_area.width());
    if (window_bounds.x() == work_area.x()) {
      if (new_x < window_bounds.right()) {
        ApplyInsets(window,
                    gfx::Insets().set_right(window_bounds.right() - new_x),
                    display);
        break;
      }
    } else if (new_x < window_bounds.x()) {
      ApplyInsets(window, gfx::Insets().set_left(new_x - window_bounds.x()),
                  display);
      break;
    }
  }
}

void WindowTilingController::OnTilingResizeRight(aura::Window* window) {
  const gfx::Rect window_bounds = window->GetBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect work_area = display.work_area();

  // "Snap" right if neither left nor right edges are against work area bounds.
  if (window_bounds.x() != work_area.x() &&
      window_bounds.right() != work_area.right()) {
    SetWindowBoundsInScreen(
        window,
        gfx::Rect(work_area.x() + work_area.width() * kInitialTilingRatio,
                  work_area.y(), work_area.width() * kInitialTilingRatio,
                  work_area.height()),
        display);
    return;
  }

  // Resizing right increases X, so forward iterate through ratios.
  for (float ratio : kTilingRatios) {
    int new_x = work_area.x() + base::ClampRound(ratio * work_area.width());
    if (window_bounds.right() == work_area.right()) {
      if (new_x > window_bounds.x()) {
        ApplyInsets(window, gfx::Insets().set_left(new_x - window_bounds.x()),
                    display);
        break;
      }
    } else if (new_x > window_bounds.right()) {
      ApplyInsets(window,
                  gfx::Insets().set_right(window_bounds.right() - new_x),
                  display);
      break;
    }
  }
}

void WindowTilingController::OnTilingResizeUp(aura::Window* window) {
  const gfx::Rect window_bounds = window->GetBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect work_area = display.work_area();

  // "Snap" up if neither top nor bottom edges are against work area bounds.
  if (window_bounds.y() != work_area.y() &&
      window_bounds.bottom() != work_area.bottom()) {
    SetWindowBoundsInScreen(
        window,
        gfx::Rect(work_area.x(), work_area.y(), work_area.width(),
                  work_area.height() * kInitialTilingRatio),
        display);
    return;
  }

  // Resizing up decreases Y, so reverse iterate through ratios.
  for (float ratio : base::Reversed(kTilingRatios)) {
    int new_y = work_area.y() + base::ClampRound(ratio * work_area.height());
    if (window_bounds.y() == work_area.y()) {
      if (new_y < window_bounds.bottom()) {
        ApplyInsets(window,
                    gfx::Insets().set_bottom(window_bounds.bottom() - new_y),
                    display);
        break;
      }
    } else if (new_y < window_bounds.y()) {
      ApplyInsets(window, gfx::Insets().set_top(new_y - window_bounds.y()),
                  display);
      break;
    }
  }
}

void WindowTilingController::OnTilingResizeDown(aura::Window* window) {
  const gfx::Rect window_bounds = window->GetBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect work_area = display.work_area();

  // "Snap" down if neither top nor bottom edges are against work area bounds.
  if (window_bounds.y() != work_area.y() &&
      window_bounds.bottom() != work_area.bottom()) {
    SetWindowBoundsInScreen(
        window,
        gfx::Rect(work_area.x(),
                  work_area.y() + work_area.height() * kInitialTilingRatio,
                  work_area.width(), work_area.height() * kInitialTilingRatio),
        display);
    return;
  }

  // Resizing down increases Y, so forward iterate through ratios.
  for (float ratio : kTilingRatios) {
    int new_y = work_area.y() + base::ClampRound(ratio * work_area.height());
    if (window_bounds.bottom() == work_area.bottom()) {
      if (new_y > window_bounds.y()) {
        ApplyInsets(window, gfx::Insets().set_top(new_y - window_bounds.y()),
                    display);
        break;
      }
    } else if (new_y > window_bounds.bottom()) {
      ApplyInsets(window,
                  gfx::Insets().set_bottom(window_bounds.bottom() - new_y),
                  display);
      break;
    }
  }
}

}  // namespace ash
