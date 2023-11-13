// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_EVENT_HANDLER_DELEGATE_H_
#define ASH_WM_OVERVIEW_EVENT_HANDLER_DELEGATE_H_

namespace ui {
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace ash {

class OverviewItemBase;

// Defines an interface for the deletage to handle events forwarded from
// `OverviewItemView`.
class EventHandlerDelegate {
 public:
  // Handles the mouse `event` from the `event_source_item`.
  virtual void HandleMouseEvent(const ui::MouseEvent& event,
                                OverviewItemBase* event_source_item) = 0;

  // Handles the gesture `event` from the `event_source_item`.
  virtual void HandleGestureEvent(ui::GestureEvent* event,
                                  OverviewItemBase* event_source_item) = 0;

 protected:
  virtual ~EventHandlerDelegate() = default;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_EVENT_HANDLER_DELEGATE_H_
