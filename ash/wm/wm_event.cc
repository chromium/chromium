// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_event.h"

namespace ash {

WMEvent::WMEvent(WMEventType type) : type_(type) {
  DCHECK(IsWorkspaceEvent() || IsCompoundEvent() || IsBoundsEvent() ||
         IsTransitionEvent());
}

WMEvent::~WMEvent() = default;

bool WMEvent::IsWorkspaceEvent() const {
  switch (type_) {
    case WM_EVENT_ADDED_TO_WORKSPACE:
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED:
    case WM_EVENT_SYSTEM_UI_AREA_CHANGED:
      return true;
    default:
      break;
  }
  return false;
}

bool WMEvent::IsCompoundEvent() const {
  switch (type_) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case WM_EVENT_TOGGLE_MAXIMIZE:
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_FULLSCREEN:
    case WM_EVENT_CYCLE_SNAP_LEFT:
    case WM_EVENT_CYCLE_SNAP_RIGHT:
      return true;
    default:
      break;
  }
  return false;
}

bool WMEvent::IsPinEvent() const {
  switch (type_) {
    case WM_EVENT_PIN:
    case WM_EVENT_TRUSTED_PIN:
      return true;
    default:
      break;
  }
  return false;
}

bool WMEvent::IsBoundsEvent() const {
  switch (type_) {
    case WM_EVENT_SET_BOUNDS:
    case WM_EVENT_CENTER:
      return true;
    default:
      break;
  }
  return false;
}

bool WMEvent::IsTransitionEvent() const {
  switch (type_) {
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
    case WM_EVENT_MINIMIZE:
    case WM_EVENT_FULLSCREEN:
    case WM_EVENT_SNAP_LEFT:
    case WM_EVENT_SNAP_RIGHT:
    case WM_EVENT_SHOW_INACTIVE:
    case WM_EVENT_PIN:
    case WM_EVENT_TRUSTED_PIN:
    case WM_EVENT_PIP:
      return true;
    default:
      break;
  }
  return false;
}

const DisplayMetricsChangedWMEvent* WMEvent::AsDisplayMetricsChangedWMEvent()
    const {
  DCHECK_EQ(type(), WM_EVENT_DISPLAY_BOUNDS_CHANGED);
  return static_cast<const DisplayMetricsChangedWMEvent*>(this);
}

SetBoundsWMEvent::SetBoundsWMEvent(const gfx::Rect& bounds,
                                   bool animate,
                                   base::TimeDelta duration)
    : WMEvent(WM_EVENT_SET_BOUNDS),
      requested_bounds_(bounds),
      animate_(animate),
      duration_(duration) {}

SetBoundsWMEvent::SetBoundsWMEvent(const gfx::Rect& requested_bounds,
                                   int64_t display_id)
    : WMEvent(WM_EVENT_SET_BOUNDS),
      requested_bounds_(requested_bounds),
      display_id_(display_id),
      animate_(false) {}

SetBoundsWMEvent::~SetBoundsWMEvent() = default;

DisplayMetricsChangedWMEvent::DisplayMetricsChangedWMEvent(int changed_metrics)
    : WMEvent(WM_EVENT_DISPLAY_BOUNDS_CHANGED),
      changed_metrics_(changed_metrics) {}

DisplayMetricsChangedWMEvent::~DisplayMetricsChangedWMEvent() = default;

}  // namespace ash
