// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_CENTER_SCROLL_BAR_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_CENTER_SCROLL_BAR_H_

#include <memory>

#include "ash/controls/rounded_scroll_bar.h"
#include "ui/events/event.h"

namespace ui {
class PresentationTimeRecorder;
}

namespace ash {

// The scroll bar for message center. This is basically just a RoundedScrollBar
// but also records the metrics for the type of scrolling (only the first event
// after the message center opens is recorded) and scrolling performance.
class MessageCenterScrollBar : public RoundedScrollBar {
  METADATA_HEADER(MessageCenterScrollBar, RoundedScrollBar)

 public:
  MessageCenterScrollBar();

  MessageCenterScrollBar(const MessageCenterScrollBar&) = delete;
  MessageCenterScrollBar& operator=(const MessageCenterScrollBar&) = delete;

  ~MessageCenterScrollBar() override;

 private:
  // RoundedScrollBar:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ScrollDelegate overrides:
  bool OnScroll(float dx, float dy) override;

  // False if no event is recorded yet. True if the first event is recorded.
  bool stats_recorded_ = false;

  // Presentation time recorder for scrolling through notification list.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_CENTER_SCROLL_BAR_H_
