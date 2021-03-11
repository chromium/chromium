// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/laser/laser_pointer_controller.h"

#include <memory>

#include "ash/fast_ink/laser/laser_pointer_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "base/bind.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

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

void LaserPointerController::SetEnabled(bool enabled) {
  if (enabled == is_enabled())
    return;

  FastInkPointerController::SetEnabled(enabled);
  if (!enabled)
    DestroyPointerView();
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
      base::TimeDelta::FromMilliseconds(kPointLifeDurationMs),
      presentation_delay,
      base::TimeDelta::FromMilliseconds(kAddStationaryPointsDelayMs),
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
}

void LaserPointerController::UpdatePointerView(ui::TouchEvent* event) {
  LaserPointerView* laser_pointer_view = GetLaserPointerView();
  laser_pointer_view->AddNewPoint(event->root_location_f(),
                                  event->time_stamp());
  if (event->type() == ui::ET_TOUCH_RELEASED) {
    laser_pointer_view->FadeOut(base::BindOnce(
        &LaserPointerController::DestroyPointerView, base::Unretained(this)));
  }
}

void LaserPointerController::DestroyPointerView() {
  laser_pointer_view_widget_.reset();
}

bool LaserPointerController::CanStartNewGesture(ui::TouchEvent* event) {
  // Ignore events over the palette.
  if (palette_utils::PaletteContainsPointInScreen(event->root_location()))
    return false;
  return FastInkPointerController::CanStartNewGesture(event);
}

void LaserPointerController::NotifyStateChanged(bool enabled) {
  for (LaserPointerObserver& observer : observers_)
    observer.OnLaserPointerStateChanged(enabled);
}

LaserPointerView* LaserPointerController::GetLaserPointerView() const {
  return static_cast<LaserPointerView*>(GetPointerView());
}

}  // namespace ash
