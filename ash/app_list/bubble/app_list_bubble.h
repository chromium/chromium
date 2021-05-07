// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class BubbleEventFilter;

// Manages the UI for the bubble launcher used in clamshell mode. Handles
// showing and hiding the UI. Only one bubble can be visible at a time, across
// all displays.
class ASH_EXPORT AppListBubble {
 public:
  AppListBubble();
  ~AppListBubble();

  // Shows the bubble on the display with `display_id`.
  void Show(int64_t display_id);

  // Shows or hides the bubble on the display with `display_id`.
  void Toggle(int64_t display_id);

  // Closes and destroys the bubble.
  void Dismiss();

  // Returns true if the bubble is showing on any display.
  bool IsShowing() const;

  views::Widget* bubble_widget_for_test() { return bubble_widget_.get(); }

 private:
  views::UniqueWidgetPtr bubble_widget_;

  // Closes the widget when the user clicks outside of it.
  std::unique_ptr<BubbleEventFilter> bubble_event_filter_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_H_
