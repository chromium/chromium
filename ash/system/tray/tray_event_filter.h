// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include <set>

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}

namespace ash {
class TrayBubbleBase;

// Handles events for a tray bubble, e.g. to close the system tray bubble when
// the user clicks outside it.
class ASH_EXPORT TrayEventFilter : public ui::EventHandler {
 public:
  TrayEventFilter();

  TrayEventFilter(const TrayEventFilter&) = delete;
  TrayEventFilter& operator=(const TrayEventFilter&) = delete;

  ~TrayEventFilter() override;

  void AddBubble(TrayBubbleBase* bubble);
  void RemoveBubble(TrayBubbleBase* bubble);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  std::set<TrayBubbleBase*> bubbles_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
