// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_event_filter.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/container_finder.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListBubbleEventFilter::AppListBubbleEventFilter(
    views::Widget* bubble_widget,
    views::View* button,
    OnClickedOutsideCallback on_click_outside)
    : BubbleEventFilter(bubble_widget, button, on_click_outside) {
  CHECK(bubble_widget);
  CHECK(on_click_outside);
}

AppListBubbleEventFilter::~AppListBubbleEventFilter() = default;

bool AppListBubbleEventFilter::ShouldRunOnClickOutsideCallback(
    const ui::LocatedEvent& event) {
  if (!BubbleEventFilter::ShouldRunOnClickOutsideCallback(event)) {
    return false;
  }

  if (aura::Window* const target = static_cast<aura::Window*>(event.target())) {
    // Don't dismiss the auto-hide shelf if event happened in status area. Then
    // the event can still be propagated.
    Shelf* shelf = Shelf::ForWindow(target);
    const aura::Window* status_window =
        shelf->shelf_widget()->status_area_widget()->GetNativeWindow();
    if (status_window && status_window->Contains(target)) {
      return false;
    }
  }

  return true;
}

}  // namespace ash
