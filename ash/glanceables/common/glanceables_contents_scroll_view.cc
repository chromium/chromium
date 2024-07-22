// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_contents_scroll_view.h"

#include <limits>
#include <optional>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/layout/flex_layout_types.h"

namespace ash {

namespace {

constexpr base::TimeDelta kMouseWheelOverscrollDelay = base::Milliseconds(150);
constexpr base::TimeDelta kScrollLockDelay = base::Milliseconds(400);

}  // namespace

class GlanceablesContentsScrollView::ScrollBar : public RoundedScrollBar {
  METADATA_HEADER(ScrollBar, RoundedScrollBar)
 public:
  explicit ScrollBar(TimeManagementContext context)
      : RoundedScrollBar(Orientation::kVertical),
        time_management_context_(context),
        mouse_wheel_overscroll_timer_(
            FROM_HERE,
            kMouseWheelOverscrollDelay,
            base::BindRepeating(&ScrollBar::OnMouseWheelTimerFired,
                                base::Unretained(this))) {}
  ScrollBar(const ScrollBar&) = delete;
  ScrollBar& operator=(const ScrollBar&) = delete;
  ~ScrollBar() override = default;

  // views::ScrollBar:
  void OnGestureEvent(ui::GestureEvent* event) override {
    // For simplicity, reset the timer for mouse wheel event so that
    // overscrolling check with mouse wheel always triggers by itself.
    ResetMouseWheelTimer();

    switch (event->type()) {
      case ui::EventType::kGestureScrollBegin:
        // Check if the position is at the max/min position at the start of the
        // scroll event.
        if (!GetVisible()) {
          // If the scrollbar is not visible, which means that the scroll view
          // is not scrollable, consider the scroll bar is at the max/min
          // position at the same time.
          scroll_starts_at_max_position_ = true;
          scroll_starts_at_min_position_ = true;
        } else {
          scroll_starts_at_min_position_ = GetPosition() == 0;

          // `GetMaxPosition()` in ScrollBar has different "position"
          // definitions from `GetPosition()`. Calculate the max position of
          // the thumb in the scrollbar for comparisons.
          scroll_starts_at_max_position_ =
              GetPosition() ==
              GetTrackBounds().height() - GetThumb()->GetLength();
        }
        break;
      case ui::EventType::kGestureScrollUpdate:
        switch (time_management_context_) {
          case TimeManagementContext::kTasks: {
            // Note that max position is at the bottom of the scrollbar, while
            // the event y offset is increasing upward.
            const bool start_overscrolling_downward =
                scroll_starts_at_max_position_ &&
                event->details().scroll_y() < 0;

            if (start_overscrolling_downward) {
              on_overscroll_callback_.Run();
            }
          } break;
          case TimeManagementContext::kClassroom: {
            const bool start_overscrolling_upward =
                scroll_starts_at_min_position_ &&
                event->details().scroll_y() > 0;

            if (start_overscrolling_upward) {
              on_overscroll_callback_.Run();
            }
          } break;
        }

        // Reset the variables for the next scroll event.
        scroll_starts_at_max_position_ = false;
        scroll_starts_at_min_position_ = false;
        break;
      default:
        // Reset the variables for the next scroll event.
        scroll_starts_at_max_position_ = false;
        scroll_starts_at_min_position_ = false;
        break;
    }

    RoundedScrollBar::OnGestureEvent(event);
  }
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override {
    const bool scroll_toward_classroom =
        time_management_context_ == TimeManagementContext::kTasks &&
        event.y_offset() < 0;
    const bool scroll_toward_tasks =
        time_management_context_ == TimeManagementContext::kClassroom &&
        event.y_offset() > 0;

    // Handle the case when the scroll view is not scrollable.
    if (!GetVisible()) {
      if (scroll_toward_classroom || scroll_toward_tasks) {
        on_overscroll_callback_.Run();
        return true;
      } else {
        return RoundedScrollBar::OnMouseWheel(event);
      }
    }

    const bool is_at_min_position = GetPosition() == 0;
    const bool is_at_max_position =
        GetPosition() == GetTrackBounds().height() - GetThumb()->GetLength();

    bool scroll_to_other_bubble =
        (scroll_toward_classroom && is_at_max_position) ||
        (scroll_toward_tasks && is_at_min_position);

    if (!scroll_to_other_bubble) {
      // Reset timer if the event is not scrolling to the other time management
      // bubble.
      ResetMouseWheelTimer();
      return RoundedScrollBar::OnMouseWheel(event);
    }

    if (can_overscroll_for_mouse_wheel_) {
      // Trigger `on_overscroll_callback_` if the timer is fired.
      on_overscroll_callback_.Run();
      ResetMouseWheelTimer();
      return true;
    }

    if (!mouse_wheel_overscroll_timer_.IsRunning()) {
      // Start the timer if the mouse wheel is scrolling toward the other time
      // management bubble and the timer wasn't fired before.
      mouse_wheel_overscroll_timer_.Reset();
    }

    return RoundedScrollBar::OnMouseWheel(event);
  }

  bool MaybeHandleScrollEvent(ui::ScrollEvent* event) {
    // For simplicity, reset the timer for mouse wheel event so that
    // overscrolling with mouse wheel always triggers by itself.
    ResetMouseWheelTimer();

    switch (event->type()) {
      case ui::EventType::kScrollFlingCancel:
        // Check if the position is at the max/min position at the start of the
        // scroll event.
        if (!GetVisible()) {
          // If the scrollbar is not visible, which means that the scroll view
          // is not scrollable, consider the scroll bar is at the max/min
          // position at the same time.
          scroll_starts_at_max_position_ = true;
          scroll_starts_at_min_position_ = true;
        } else {
          scroll_starts_at_min_position_ = GetPosition() == 0;

          // `GetMaxPosition()` in ScrollBar has different "position"
          // definitions from `GetPosition()`. Calculate the max position of
          // the thumb in the scrollbar for comparisons.
          scroll_starts_at_max_position_ =
              GetPosition() ==
              GetTrackBounds().height() - GetThumb()->GetLength();
        }
        break;
      case ui::EventType::kScroll:
        switch (time_management_context_) {
          case TimeManagementContext::kTasks: {
            // Note that max position is at the bottom of the scrollbar, while
            // the event y offset is increasing upward.
            const bool start_overscrolling_downward =
                scroll_starts_at_max_position_ && event->y_offset() < 0;

            if (start_overscrolling_downward) {
              on_overscroll_callback_.Run();
              return true;
            }
          } break;
          case TimeManagementContext::kClassroom: {
            const bool start_overscrolling_upward =
                scroll_starts_at_min_position_ && event->y_offset() > 0;

            if (start_overscrolling_upward) {
              on_overscroll_callback_.Run();
              return true;
            }
          } break;
        }

        // Reset the variables for the next scroll event.
        scroll_starts_at_max_position_ = false;
        scroll_starts_at_min_position_ = false;
        break;
      default:
        // Reset the variables for the next scroll event.
        scroll_starts_at_max_position_ = false;
        scroll_starts_at_min_position_ = false;
        break;
    }

    return false;
  }

  void SetOnOverscrollCallback(const base::RepeatingClosure& callback) {
    on_overscroll_callback_ = std::move(callback);
  }

  void FireMouseWheelTimerForTest() {
    if (mouse_wheel_overscroll_timer_.IsRunning()) {
      mouse_wheel_overscroll_timer_.Stop();
      OnMouseWheelTimerFired();
    }
  }

 private:
  void OnMouseWheelTimerFired() { can_overscroll_for_mouse_wheel_ = true; }

  void ResetMouseWheelTimer() {
    can_overscroll_for_mouse_wheel_ = false;
    mouse_wheel_overscroll_timer_.Stop();
  }

  // Whether the glanceables that owns this scroll view is Tasks or Classroom.
  // This will determine the overscroll behavior that triggers
  // `on_overscroll_callback_`.
  const TimeManagementContext time_management_context_;

  // Whether the scroll bar is at its max position, which is bottom in this
  // case, when a scroll event started.
  bool scroll_starts_at_max_position_ = false;

  // Whether the scroll bar is at its min position, which is top in this
  // case, when a scroll event started.
  bool scroll_starts_at_min_position_ = false;

  // Called when the user attempts to overscroll to the direction that has
  // another glanceables. Overscroll here means to either scroll down from the
  // bottom of the scroll view, or scroll up from the top of the scroll view.
  base::RepeatingClosure on_overscroll_callback_ = base::DoNothing();

  // As there is no explicit "start" and "end" of a mouse wheel scroll action, a
  // timer is used to determine if a mouse scroll should trigger
  // `on_overscroll_callback_`. Once the timer is fired, set
  // `can_overscroll_for_mouse_wheel_` to true to trigger
  // `on_overscroll_callback_` in subsequent mouse scroll events.
  base::RetainingOneShotTimer mouse_wheel_overscroll_timer_;
  bool can_overscroll_for_mouse_wheel_ = false;
};

BEGIN_METADATA(GlanceablesContentsScrollView, ScrollBar)
END_METADATA

GlanceablesContentsScrollView::GlanceablesContentsScrollView(
    TimeManagementContext context)
    : scroll_lock_timer_(
          FROM_HERE,
          kScrollLockDelay,
          base::BindRepeating(
              &GlanceablesContentsScrollView::OnScrollLockTimerFired,
              base::Unretained(this))) {
  auto unique_scroll_bar = std::make_unique<ScrollBar>(context);
  scroll_bar_ = unique_scroll_bar.get();
  SetVerticalScrollBar(std::move(unique_scroll_bar));

  SetID(base::to_underlying(GlanceablesViewId::kContentsScrollView));
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded)
                  .WithWeight(1));
  ClipHeightTo(0, std::numeric_limits<int>::max());
  SetBackgroundColor(std::nullopt);
  SetDrawOverflowIndicator(false);
}

void GlanceablesContentsScrollView::SetOnOverscrollCallback(
    const base::RepeatingClosure& callback) {
  scroll_bar_->SetOnOverscrollCallback(std::move(callback));
}

void GlanceablesContentsScrollView::LockScroll() {
  if (scroll_lock_) {
    return;
  }

  scroll_lock_ = true;
  scroll_lock_timer_.Reset();
}

void GlanceablesContentsScrollView::UnlockScroll() {
  scroll_lock_ = false;
  scroll_lock_timer_.Stop();
}

void GlanceablesContentsScrollView::FireScrollLockTimerForTest() {
  scroll_lock_timer_.Stop();
  OnScrollLockTimerFired();
}

void GlanceablesContentsScrollView::FireMouseWheelTimerForTest() {
  scroll_bar_->FireMouseWheelTimerForTest();  // IN-TEST
}

void GlanceablesContentsScrollView::OnScrollEvent(ui::ScrollEvent* event) {
  views::ScrollView::OnScrollEvent(event);

  bool handled = scroll_bar_->MaybeHandleScrollEvent(event);
  if (handled) {
    event->SetHandled();
    event->StopPropagation();
  }

  // Not handling `scroll_lock_` as unhandled scroll event will be transferred
  // to a mouse wheel event in `Widget`.
}

bool GlanceablesContentsScrollView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  if (scroll_lock_ && scroll_bar_->GetVisible()) {
    // Lock the scrolling by returning true without processing the event.
    return true;
  }

  // Always pass the event to `scroll_bar_` to check the overscroll.
  return scroll_bar_->OnMouseWheel(e);
}

void GlanceablesContentsScrollView::OnGestureEvent(ui::GestureEvent* event) {
  // views::ScrollView::OnGestureEvent only processes the scroll event when the
  // scroll bar is visible and the scroll view is scrollable, but the overscroll
  // behavior also needs to consider the case where the scroll view is not
  // scrollable.
  bool scroll_event = event->type() == ui::EventType::kGestureScrollUpdate ||
                      event->type() == ui::EventType::kGestureScrollBegin ||
                      event->type() == ui::EventType::kGestureScrollEnd ||
                      event->type() == ui::EventType::kScrollFlingStart;

  if (!scroll_bar_->GetVisible() && scroll_event) {
    scroll_bar_->OnGestureEvent(event);
    return;
  }

  views::ScrollView::OnGestureEvent(event);
}

void GlanceablesContentsScrollView::ChildPreferredSizeChanged(
    views::View* view) {
  PreferredSizeChanged();
}

void GlanceablesContentsScrollView::OnScrollLockTimerFired() {
  scroll_lock_ = false;
}

BEGIN_METADATA(GlanceablesContentsScrollView)
END_METADATA

}  // namespace ash
