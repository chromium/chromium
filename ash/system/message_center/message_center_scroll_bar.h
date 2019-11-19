// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_

#include "ui/events/event.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

namespace ash {

// The scroll bar for message center. This is basically views::OverlayScrollBar
// but also records the metrics for the type of scrolling. Only the first event
// after the message center opens is recorded.
class MessageCenterScrollBar : public views::OverlayScrollBar {
 public:
  class Observer {
   public:
    // Called when scroll event is triggered.
    virtual void OnMessageCenterScrolled() = 0;
    virtual ~Observer() = default;
  };

  // |observer| can be null.
  explicit MessageCenterScrollBar(Observer* observer);

 private:
  // View overrides:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  const char* GetClassName() const override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ScrollDelegate overrides:
  bool OnScroll(float dx, float dy) override;

  // False if no event is recorded yet. True if the first event is recorded.
  bool stats_recorded_ = false;

  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterScrollBar);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
