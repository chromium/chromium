// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_

#include <memory>

#include "ui/events/event_handler.h"

namespace ash {
class WmGestureHandler;

// An event filter which handles system level gesture events.
class SystemGestureEventFilter : public ui::EventHandler {
 public:
  SystemGestureEventFilter();

  SystemGestureEventFilter(const SystemGestureEventFilter&) = delete;
  SystemGestureEventFilter& operator=(const SystemGestureEventFilter&) = delete;

  ~SystemGestureEventFilter() override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

 private:
  friend class SystemGestureEventFilterTest;

  std::unique_ptr<WmGestureHandler> wm_gesture_handler_;
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
