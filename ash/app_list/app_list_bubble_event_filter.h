// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_BUBBLE_EVENT_FILTER_H_
#define ASH_APP_LIST_APP_LIST_BUBBLE_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/bubble/bubble_event_filter.h"
#include "base/functional/callback.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// Handles events for the app list bubble, the given callback will be triggered
// when an event happens and `ShouldRunOnClickOutsideCallback()` is satisfied.
class ASH_EXPORT AppListBubbleEventFilter : public BubbleEventFilter {
 public:
  AppListBubbleEventFilter(views::Widget* bubble_widget,
                           views::View* button,
                           OnClickedOutsideCallback on_click_outside);
  AppListBubbleEventFilter(const AppListBubbleEventFilter&) = delete;
  AppListBubbleEventFilter& operator=(const AppListBubbleEventFilter&) = delete;
  ~AppListBubbleEventFilter() override;

  // BubbleEventFilter
  bool ShouldRunOnClickOutsideCallback(const ui::LocatedEvent& event) override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_BUBBLE_EVENT_FILTER_H_
