// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_CONTENTS_SCROLL_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_CONTENTS_SCROLL_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class ASH_EXPORT GlanceablesContentsScrollView : public views::ScrollView {
  METADATA_HEADER(GlanceablesContentsScrollView, views::ScrollView)
 public:
  using TimeManagementContext = GlanceablesTimeManagementBubbleView::Context;

  explicit GlanceablesContentsScrollView(TimeManagementContext context);
  GlanceablesContentsScrollView(const GlanceablesContentsScrollView&) = delete;
  GlanceablesContentsScrollView& operator=(
      const GlanceablesContentsScrollView&) = delete;
  ~GlanceablesContentsScrollView() override = default;

  // Sets the callback to call when overscroll happens. Overscroll here means to
  // either scroll down from the bottom of the scroll view, or scroll up from
  // the top of the scroll view.
  void SetOnOverscrollCallback(const base::RepeatingClosure& callback);

  // Fires the timer for mouse wheel overscroll so that next mouse wheel event
  // will expand the other glanceables. This is only used in tests.
  void FireMouseWheelTimerForTest();

  // Locks the scroll view so that scrolling action does not actually scroll the
  // contents of the scroll view.
  void LockScroll();
  // Similar to `LockScroll()`, but unlocks the scroll view to allow scrolling.
  void UnlockScroll();
  void FireScrollLockTimerForTest();

  // views::ScrollView:
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& e) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ChildPreferredSizeChanged(views::View* view) override;

 private:
  class ScrollBar;

  void OnScrollLockTimerFired();

  raw_ptr<ScrollBar> scroll_bar_ = nullptr;

  // To prevent the scroll view scrolling right after overscrolling from another
  // `GlanceablesContentsScrollView`, use a timer here to disable the scrolling
  // for a short amount of time.
  base::RetainingOneShotTimer scroll_lock_timer_;
  bool scroll_lock_ = false;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_CONTENTS_SCROLL_VIEW_H_
