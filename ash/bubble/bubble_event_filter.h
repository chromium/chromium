// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_EVENT_FILTER_H_
#define ASH_BUBBLE_BUBBLE_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// Observes mouse and touch events and invokes a callback when
// `ShouldRunOnClickOutsideCallback()` is satisfied.  For example, this class
// can be used to close the bubble according to an appropriate event.
class ASH_EXPORT BubbleEventFilter : public ui::EventHandler {
 public:
  using OnClickedOutsideCallback =
      base::RepeatingCallback<void(const ui::LocatedEvent&)>;

  // See class comment. Runs `on_click_outside` when a click or tap occurs
  // outside the bounds of `bubble_widget` and `button`.
  BubbleEventFilter(views::Widget* bubble_widget,
                    views::View* button,
                    OnClickedOutsideCallback on_click_outside);
  BubbleEventFilter(const BubbleEventFilter&) = delete;
  BubbleEventFilter& operator=(const BubbleEventFilter&) = delete;
  ~BubbleEventFilter() override;

  // Changes the button to be checked in `ShouldRunOnClickOutsideCallback()`.
  // This will be used for the app list bubble where the app list has changed
  // displays, so a different home button needs to be checked.
  void SetButton(views::View* button);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 protected:
  // Checks whether we should run `on_click_outside_` or not.
  virtual bool ShouldRunOnClickOutsideCallback(const ui::LocatedEvent& event);

  void ProcessPressedEvent(const ui::LocatedEvent& event);

 private:
  const raw_ptr<views::Widget> bubble_widget_;
  raw_ptr<views::View, DanglingUntriaged> button_;  // May be null.
  OnClickedOutsideCallback on_click_outside_;
};

}  // namespace ash

#endif  // ASH_BUBBLE_BUBBLE_EVENT_FILTER_H_
