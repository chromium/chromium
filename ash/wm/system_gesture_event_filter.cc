// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/wm/gestures/wm_gesture_handler.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {

SystemGestureEventFilter::SystemGestureEventFilter()
    : wm_gesture_handler_(std::make_unique<WmGestureHandler>()) {}

SystemGestureEventFilter::~SystemGestureEventFilter() = default;

void SystemGestureEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed &&
      ui::GetTouchScreensAvailability() ==
          ui::TouchScreensAvailability::ENABLED) {
    base::RecordAction(base::UserMetricsAction("Mouse_Down"));
  }
}

void SystemGestureEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
  if (wm_gesture_handler_ && wm_gesture_handler_->ProcessScrollEvent(*event))
    event->StopPropagation();
}

}  // namespace ash
