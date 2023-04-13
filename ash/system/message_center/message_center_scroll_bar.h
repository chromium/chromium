// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_

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
// TODO(b/257291597): Rename this file to match the class name.
class RoundedMessageCenterScrollBar : public RoundedScrollBar {
 public:
  METADATA_HEADER(RoundedMessageCenterScrollBar);

  class Observer {
   public:
    // Called when scroll event is triggered.
    virtual void OnMessageCenterScrolled() = 0;
    virtual ~Observer() = default;
  };

  // |observer| can be null.
  explicit RoundedMessageCenterScrollBar(Observer* observer);

  RoundedMessageCenterScrollBar(const RoundedMessageCenterScrollBar&) = delete;
  RoundedMessageCenterScrollBar& operator=(
      const RoundedMessageCenterScrollBar&) = delete;

  ~RoundedMessageCenterScrollBar() override;

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

  // Unowned.
  Observer* const observer_;

  // Presentation time recorder for scrolling through notification list.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
