// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ui {
class Event;
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace ash {
class OverviewGrid;
class OverviewSession;
class WmFlingHandler;

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
//   - Scrolling through tablet overview mode on scrolling.
//   - Scrolling through tablet overview mode on flinging.
class OverviewGridEventHandler : public ui::EventHandler {
 public:
  explicit OverviewGridEventHandler(OverviewGrid* grid);
  OverviewGridEventHandler(const OverviewGridEventHandler&) = delete;
  OverviewGridEventHandler& operator=(const OverviewGridEventHandler&) = delete;
  ~OverviewGridEventHandler() override;

  void HandleClickOrTap(ui::Event* event);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  bool IsFlingInProgressForTesting() const { return !!fling_handler_; }

 private:
  void HandleFlingScroll(ui::GestureEvent* event);

  bool OnFlingStep(float offset);
  void OnFlingEnd();

  // Cached value of the OverviewGrid that handles a series of gesture scroll
  // events. Guaranteed to be alive during the lifetime of |this|.
  raw_ptr<OverviewGrid> grid_;

  // Guaranteed to be alive during the lifetime of |this|.
  const raw_ptr<OverviewSession> overview_session_;

  // The cumulative scroll offset. This is used so that tiny scrolls will not
  // make minuscule shifts on the grid, but are not completely ignored.
  float scroll_offset_x_cumulative_ = 0.f;

  // Fling handler of the current active fling. Nullptr while a fling is not
  // active.
  std::unique_ptr<WmFlingHandler> fling_handler_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_EVENT_HANDLER_H_
