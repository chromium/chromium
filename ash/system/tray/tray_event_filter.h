// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Handles events for a tray bubble, e.g. to close the system tray bubble when
// the user clicks outside it.
class ASH_EXPORT TrayEventFilter : public ui::EventHandler {
 public:
  TrayEventFilter(views::Widget* bubble_widget,
                  TrayBubbleView* bubble_view,
                  TrayBackgroundView* tray_button);

  TrayEventFilter(const TrayEventFilter&) = delete;
  TrayEventFilter& operator=(const TrayEventFilter&) = delete;

  ~TrayEventFilter() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  const raw_ptr<views::Widget> bubble_widget_;
  const raw_ptr<TrayBubbleView> bubble_view_;
  const raw_ptr<TrayBackgroundView> tray_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
