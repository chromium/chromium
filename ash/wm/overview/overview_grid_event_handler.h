// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class Compositor;
class Event;
class FlingCurve;
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace ash {
class OverviewGrid;

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
//   - Scrolling through tablet overview mode on scrolling.
//   - Scrolling through tablet overview mode on flinging.
class OverviewGridEventHandler : public ui::EventHandler,
                                 public ui::CompositorAnimationObserver {
 public:
  explicit OverviewGridEventHandler(OverviewGrid* grid);
  ~OverviewGridEventHandler() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  bool IsFlingInProgressForTesting() const { return !!fling_curve_; }

 private:
  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void HandleClickOrTap(ui::Event* event);

  void HandleFlingScroll(ui::GestureEvent* event);

  void EndFling();

  // Cached value of the OverviewGrid that handles a series of gesture scroll
  // events. Guaranteed to be alive during the lifetime of |this|.
  OverviewGrid* grid_;

  // Gesture curve of the current active fling. nullptr while a fling is not
  // active.
  std::unique_ptr<ui::FlingCurve> fling_curve_;

  // Velocity of the fling that will gradually decrease during a fling.
  gfx::Vector2dF fling_velocity_;

  // Cached value of an earlier offset that determines values to scroll through
  // overview mode by being compared to an updated offset.
  base::Optional<gfx::Vector2dF> fling_last_offset_;

  // The compositor we are observing when a fling is underway.
  ui::Compositor* observed_compositor_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OverviewGridEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_
