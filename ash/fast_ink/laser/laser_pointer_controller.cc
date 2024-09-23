// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/laser/laser_pointer_controller.h"

#include <memory>

#include "ash/fast_ink/laser/laser_pointer_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// A point gets removed from the collection if it is older than
// |kPointLifeDurationMs|.
const int kPointLifeDurationMs = 200;

// When no move events are being received we add a new point every
// |kAddStationaryPointsDelayMs| so that points older than
// |kPointLifeDurationMs| can get removed.
// Note: Using a delay less than the screen refresh interval will not
// provide a visual benefit but instead just waste time performing
// unnecessary updates. 16ms is the refresh interval on most devices.
// TODO(reveman): Use real VSYNC interval instead of a hard-coded delay.
const int kAddStationaryPointsDelayMs = 16;

}  // namespace

// A class to hide and lock mouse cursor while it is alive.
class LaserPointerController::ScopedLockedHiddenCursor {
 public:
  ScopedLockedHiddenCursor() : cursor_manager_(Shell::Get()->cursor_manager()) {
    DCHECK(cursor_manager_);
    // Hide and lock the cursor.
    cursor_manager_->HideCursor();
    cursor_manager_->LockCursor();
  }
  ~ScopedLockedHiddenCursor() {
    // Unlock the cursor.
    cursor_manager_->UnlockCursor();
  }

 private:
  const raw_ptr<wm::CursorManager> cursor_manager_;
};

LaserPointerController::LaserPointerController() {
  Shell::Get()->AddPreTargetHandler(this);
}

LaserPointerController::~LaserPointerController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void LaserPointerController::AddObserver(LaserPointerObserver* observer) {
  observers_.AddObserver(observer);
}

void LaserPointerController::RemoveObserver(LaserPointerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LaserPointerController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, root_window_observation_.GetSource());

  // If the display is rotated or the device scale factor is changed during a
  // gesture, the last point segment will be in a different position on the
  // display. Since the root window bounds are synced with the display, the
  // controller will be notified if the display's rotation or device scale
  // factor changes.
  // Resetting the state of the buffer associated with the
  // `laser_pointer_view_widget_` is non-trivial, so the view is destroyed
  // instead to start with a fresh state. Since it is very rare to have an
  // ongoing gesture with display changes, we can safely assume that there is no
  // performance penalty.
  DestroyPointerView();
}

void LaserPointerController::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, root_window_observation_.GetSource());
  root_window_observation_.Reset();
}

void LaserPointerController::SetEnabled(bool enabled) {
  if (enabled == is_enabled())
    return;

  FastInkPointerController::SetEnabled(enabled);
  if (!enabled) {
    DestroyPointerView();
    // Unlock mouse cursor when disabling.
    scoped_locked_hidden_cursor_.reset();
  }
  NotifyStateChanged(enabled);
}

views::View* LaserPointerController::GetPointerView() const {
  return laser_pointer_view_widget_
             ? laser_pointer_view_widget_->GetContentsView()
             : nullptr;
}

void LaserPointerController::CreatePointerView(
    base::TimeDelta presentation_delay,
    aura::Window* root_window) {
  laser_pointer_view_widget_ = LaserPointerView::Create(
      base::Milliseconds(kPointLifeDurationMs), presentation_delay,
      base::Milliseconds(kAddStationaryPointsDelayMs),
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));

  DCHECK(!root_window_observation_.IsObserving());
  root_window_observation_.Observe(root_window);
}

void LaserPointerController::UpdatePointerView(ui::TouchEvent* event) {
  LaserPointerView* laser_pointer_view = GetLaserPointerView();

  if (IsPointerInExcludedWindows(event)) {
    // Destroy the |LaserPointerView| since the pointer is in the bound of
    // excluded windows.
    DestroyPointerView();
    return;
  }

  if (event->type() != ui::EventType::kTouchCancelled) {
    // Unlock mouse cursor when switch to touch event.
    scoped_locked_hidden_cursor_.reset();

    laser_pointer_view->AddNewPoint(event->root_location_f(),
                                    event->time_stamp());
  }

  // Upon disabling the touch display, a canceled touch event is received. The
  // `laser_pointer_view` should be destroyed to avoid visual artifacts. See
  // b/303924797 for more information.
  if (event->type() == ui::EventType::kTouchReleased ||
      event->type() == ui::EventType::kTouchCancelled) {
    laser_pointer_view->FadeOut(base::BindOnce(
        &LaserPointerController::DestroyPointerView, base::Unretained(this)));
  }
}

void LaserPointerController::UpdatePointerView(ui::MouseEvent* event) {
  LaserPointerView* laser_pointer_view = GetLaserPointerView();
  if (event->type() == ui::EventType::kMouseMoved) {
    if (IsPointerInExcludedWindows(event)) {
      // Destroy the |LaserPointerView| and unlock the cursor since the cursor
      // is in the bound of excluded windows.
      DestroyPointerView();
      scoped_locked_hidden_cursor_.reset();
      return;
    }

    if (!scoped_locked_hidden_cursor_) {
      scoped_locked_hidden_cursor_ =
          std::make_unique<ScopedLockedHiddenCursor>();
    }
  }

  laser_pointer_view->AddNewPoint(event->root_location_f(),
                                  event->time_stamp());
  if (event->type() == ui::EventType::kMouseReleased) {
    laser_pointer_view->FadeOut(base::BindOnce(
        &LaserPointerController::DestroyPointerView, base::Unretained(this)));
  }
}

void LaserPointerController::DestroyPointerView() {
  root_window_observation_.Reset();
  laser_pointer_view_widget_.reset();
}

bool LaserPointerController::CanStartNewGesture(ui::LocatedEvent* event) {
  // Ignore events over the palette.
  // TODO(llin): Register palette as a excluded window instead.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point screen_point = event->location();
  wm::ConvertPointToScreen(target, &screen_point);
  if (palette_utils::PaletteContainsPointInScreen(screen_point))
    return false;
  return FastInkPointerController::CanStartNewGesture(event);
}

bool LaserPointerController::ShouldProcessEvent(ui::LocatedEvent* event) {
  // Allow clicking when laser pointer is enabled.
  if (event->type() == ui::EventType::kMousePressed ||
      event->type() == ui::EventType::kMouseReleased) {
    return false;
  }

  return FastInkPointerController::ShouldProcessEvent(event);
}

void LaserPointerController::NotifyStateChanged(bool enabled) {
  for (LaserPointerObserver& observer : observers_)
    observer.OnLaserPointerStateChanged(enabled);
}

LaserPointerView* LaserPointerController::GetLaserPointerView() const {
  return static_cast<LaserPointerView*>(GetPointerView());
}

}  // namespace ash
