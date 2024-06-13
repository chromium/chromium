// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_contents_scroll_view.h"

#include <limits>
#include <optional>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/layout/flex_layout_types.h"

namespace ash {

class GlanceablesContentsScrollView::ScrollBar : public RoundedScrollBar {
  METADATA_HEADER(ScrollBar, RoundedScrollBar)
 public:
  explicit ScrollBar(TimeManagementContext context)
      : RoundedScrollBar(Orientation::kVertical),
        time_management_context_(context) {}
  ScrollBar(const ScrollBar&) = delete;
  ScrollBar& operator=(const ScrollBar&) = delete;
  ~ScrollBar() override = default;

  // views::ScrollBar:
  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        // Check if the position is at the max/min position at the start of the
        // scroll event.
        if (!GetVisible()) {
          // If the scrollbar is not visible, which means that the scroll view
          // is not scrollable, consider the scroll bar is at the max/min
          // position at the same time.
          is_at_max_position_ = true;
          is_at_min_position_ = true;
        } else {
          is_at_min_position_ = GetPosition() == 0;

          // `GetMaxPosition()` in ScrollBar has different "position"
          // definitions from `GetPosition()`. Calculate the max position of
          // the thumb in the scrollbar for comparisons.
          is_at_max_position_ = GetPosition() == GetTrackBounds().height() -
                                                     GetThumb()->GetLength();
        }
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        switch (time_management_context_) {
          case TimeManagementContext::kTasks: {
            // Note that max position is at the bottom of the scrollbar, while
            // the event y offset is increasing upward.
            const bool start_overscrolling_downward =
                is_at_max_position_ && event->details().scroll_y() < 0;

            if (start_overscrolling_downward) {
              on_overscroll_callback_.Run();
            }
          } break;
          case TimeManagementContext::kClassroom: {
            const bool start_overscrolling_upward =
                is_at_min_position_ && event->details().scroll_y() > 0;

            if (start_overscrolling_upward) {
              on_overscroll_callback_.Run();
            }
          } break;
        }

        // Reset the variables for the next scroll event.
        is_at_max_position_ = false;
        is_at_min_position_ = false;
        break;
      default:
        // Reset the variables for the next scroll event.
        is_at_max_position_ = false;
        is_at_min_position_ = false;
        break;
    }

    RoundedScrollBar::OnGestureEvent(event);
  }
  void ObserveScrollEvent(const ui::ScrollEvent& event) override {
    switch (event.type()) {
      case ui::ET_SCROLL_FLING_CANCEL:
        // Check if the position is at the max/min position at the start of the
        // scroll event.
        if (!GetVisible()) {
          // If the scrollbar is not visible, which means that the scroll view
          // is not scrollable, consider the scroll bar is at the max/min
          // position at the same time.
          is_at_max_position_ = true;
          is_at_min_position_ = true;
        } else {
          is_at_min_position_ = GetPosition() == 0;

          // `GetMaxPosition()` in ScrollBar has different "position"
          // definitions from `GetPosition()`. Calculate the max position of
          // the thumb in the scrollbar for comparisons.
          is_at_max_position_ = GetPosition() == GetTrackBounds().height() -
                                                     GetThumb()->GetLength();
        }
        break;
      case ui::ET_SCROLL:
        switch (time_management_context_) {
          case TimeManagementContext::kTasks: {
            // Note that max position is at the bottom of the scrollbar, while
            // the event y offset is increasing upward.
            const bool start_overscrolling_downward =
                is_at_max_position_ && event.y_offset() < 0;

            if (start_overscrolling_downward) {
              on_overscroll_callback_.Run();
            }
          } break;
          case TimeManagementContext::kClassroom: {
            const bool start_overscrolling_upward =
                is_at_min_position_ && event.y_offset() > 0;

            if (start_overscrolling_upward) {
              on_overscroll_callback_.Run();
            }
          } break;
        }

        // Reset the variables for the next scroll event.
        is_at_max_position_ = false;
        is_at_min_position_ = false;
        break;
      default:
        // Reset the variables for the next scroll event.
        is_at_max_position_ = false;
        is_at_min_position_ = false;
        break;
    }

    RoundedScrollBar::ObserveScrollEvent(event);
  }

  void SetOnOverscrollCallback(const base::RepeatingClosure& callback) {
    on_overscroll_callback_ = std::move(callback);
  }

 private:
  // Whether the glanceables that owns this scroll view is Tasks or Classroom.
  // This will determine the overscroll behavior that triggers
  // `on_overscroll_callback_`.
  const TimeManagementContext time_management_context_;

  // Whether the scroll bar is at its max position, which is bottom in this
  // case.
  bool is_at_max_position_ = false;

  // Whether the scroll bar is at its min position, which is top in this
  // case.
  bool is_at_min_position_ = false;

  // Called when the user attempts to overscroll to the direction that has
  // another glanceables. Overscroll here means to either scroll down from the
  // bottom of the scroll view, or scroll up from the top of the scroll view.
  base::RepeatingClosure on_overscroll_callback_ = base::DoNothing();
};

BEGIN_METADATA(GlanceablesContentsScrollView, ScrollBar)
END_METADATA

GlanceablesContentsScrollView::GlanceablesContentsScrollView(
    TimeManagementContext context) {
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

void GlanceablesContentsScrollView::OnGestureEvent(ui::GestureEvent* event) {
  // views::ScrollView::OnGestureEvent only processes the scroll event when the
  // scroll bar is visible and the scroll view is scrollable, but the overscroll
  // behavior also needs to consider the case where the scroll view is not
  // scrollable.
  bool scroll_event = event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
                      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
                      event->type() == ui::ET_GESTURE_SCROLL_END ||
                      event->type() == ui::ET_SCROLL_FLING_START;

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

BEGIN_METADATA(GlanceablesContentsScrollView)
END_METADATA

}  // namespace ash
