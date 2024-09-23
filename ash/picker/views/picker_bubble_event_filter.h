// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_BUBBLE_EVENT_FILTER_H_
#define ASH_PICKER_VIEWS_PICKER_BUBBLE_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/bubble/bubble_event_filter.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Handles outside clicks for the Picker widget.
// It closes the given widget if clicking outside of the widget or any of its
// children.
class ASH_EXPORT PickerBubbleEventFilter : public BubbleEventFilter {
 public:
  // `widget` must outlive this class.
  explicit PickerBubbleEventFilter(views::Widget* widget);
  PickerBubbleEventFilter(const PickerBubbleEventFilter&) = delete;
  PickerBubbleEventFilter& operator=(const PickerBubbleEventFilter&) = delete;
  ~PickerBubbleEventFilter() override;

  // BubbleEventFilter:
  bool ShouldRunOnClickOutsideCallback(const ui::LocatedEvent& event) override;

 private:
  void OnClickOutsideWidget(const ui::LocatedEvent& event);

  raw_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_BUBBLE_EVENT_FILTER_H_
