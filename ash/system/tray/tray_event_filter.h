// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include <set>

#include "ash/ash_export.h"
#include "base/macros.h"
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
  ~TrayEventFilter() override;

  void AddBubble(TrayBubbleBase* bubble);
  void RemoveBubble(TrayBubbleBase* bubble);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  std::set<TrayBubbleBase*> bubbles_;

  DISALLOW_COPY_AND_ASSIGN(TrayEventFilter);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
