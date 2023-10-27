// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_metrics.h"

#include "ash/public/cpp/metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace ash {

namespace calendar_metrics {

namespace {

constexpr char kCalendarViewShowSourcePrefix[] = "Ash.Calendar.ShowSource.";
constexpr char kCalendarDateCellActivated[] = "Ash.Calendar.DateCell.Activated";
constexpr char kCalendarEventListItemActivated[] =
    "Ash.Calendar.EventListItem.Activated";
constexpr char kCalendarMonthDownArrowButtonActivated[] =
    "Ash.Calendar.MonthDownArrowButton.Activated";
constexpr char kCalendarMonthUpArrowButtonActivated[] =
    "Ash.Calendar.MonthUpArrowButton.Activated";
constexpr char kCalendarMonthDwellTime[] = "Ash.Calendar.MonthDwellTime";
constexpr char kCalendarScrollSource[] = "Ash.Calendar.ScrollSource";
constexpr char kCalendarKeyboardNavigation[] =
    "Ash.Calendar.KeyboardNavigation";
constexpr char kCalendarEventListItemInUpNextPressed[] =
    "Ash.Calendar.UpNextView.EventListItem.Pressed";
constexpr char kCalendarUpNextEventDisplayedCount[] =
    "Ash.Calendar.UpNextView.EventDisplayedCount";
constexpr char kCalendarEventListItemJoinButtonPressed[] =
    "Ash.Calendar.EventListView.JoinMeetingButton.Pressed";
constexpr char kCalendarUpNextJoinButtonPressed[] =
    "Ash.Calendar.UpNextView.JoinMeetingButton.Pressed";
constexpr char kCalendarEventListEventDisplayedCount[] =
    "Ash.Calendar.EventListViewJelly.EventDisplayedCount";
constexpr char kCalendarEventsDisplayedToUser[] =
    "Ash.Calendar.EventsDisplayedToUser";

}  // namespace

CalendarEventSource GetEventType(const ui::Event& event) {
  if (event.IsGestureEvent())
    return CalendarEventSource::kTap;

  if (event.IsMouseEvent())
    return CalendarEventSource::kClick;

  if (event.IsKeyEvent())
    return CalendarEventSource::kKeyboard;

  if (event.IsTouchEvent())
    return CalendarEventSource::kStylus;

  NOTREACHED();
  return CalendarEventSource::kInvalid;
}

void RecordCalendarShowMetrics(
    CalendarViewShowSource show_source,
    calendar_metrics::CalendarEventSource event_source) {
  std::string histogram_name = kCalendarViewShowSourcePrefix;
  switch (show_source) {
    case CalendarViewShowSource::kDateView:
      histogram_name += "DateView";
      break;
    case CalendarViewShowSource::kTimeView:
      histogram_name += "TimeView";
      break;
    case CalendarViewShowSource::kAccelerator:
      histogram_name += "Keyboard";
      break;
  }

  base::UmaHistogramEnumeration(histogram_name, event_source);
}

void RecordCalendarDateCellActivated(const ui::Event& event) {
  base::UmaHistogramEnumeration(kCalendarDateCellActivated,
                                GetEventType(event));
}

void RecordMonthArrowButtonActivated(bool up, const ui::Event& event) {
  base::UmaHistogramEnumeration(up ? kCalendarMonthUpArrowButtonActivated
                                   : kCalendarMonthDownArrowButtonActivated,
                                GetEventType(event));
}

void RecordEventListItemActivated(const ui::Event& event) {
  base::UmaHistogramEnumeration(kCalendarEventListItemActivated,
                                GetEventType(event));
}

void RecordMonthDwellTime(const base::TimeDelta& dwell_time) {
  base::UmaHistogramMediumTimes(kCalendarMonthDwellTime, dwell_time);
}

void RecordScrollSource(CalendarViewScrollSource source) {
  base::UmaHistogramEnumeration(kCalendarScrollSource, source);
}

ui::AnimationThroughputReporter CreateAnimationReporter(
    views::View* view,
    const std::string& animation_histogram_name) {
  // TODO(crbug.com/1297376): Add unit tests for animation metrics recording.
  return ui::AnimationThroughputReporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          [](const std::string& animation_histogram_name, int smoothness) {
            base::UmaHistogramPercentage(animation_histogram_name, smoothness);
          },
          animation_histogram_name)));
}

void RecordCalendarKeyboardNavigation(
    const CalendarKeyboardNavigationSource key_source) {
  base::UmaHistogramEnumeration(kCalendarKeyboardNavigation, key_source);
}

void RecordEventListItemInUpNextLaunched(const ui::Event& event) {
  base::UmaHistogramEnumeration(kCalendarEventListItemInUpNextPressed,
                                GetEventType(event));
}

void RecordUpNextEventCount(const int event_count) {
  base::UmaHistogramCounts100(kCalendarUpNextEventDisplayedCount, event_count);
}

void RecordJoinButtonPressedFromEventListView(const ui::Event& event) {
  base::UmaHistogramEnumeration(kCalendarEventListItemJoinButtonPressed,
                                GetEventType(event));
}

void RecordJoinButtonPressedFromUpNextView(const ui::Event& event) {
  base::UmaHistogramEnumeration(kCalendarUpNextJoinButtonPressed,
                                GetEventType(event));
}

void RecordEventListEventCount(const int event_count) {
  base::UmaHistogramCounts100(kCalendarEventListEventDisplayedCount,
                              event_count);
}

void RecordEventsDisplayedToUser() {
  base::UmaHistogramBoolean(kCalendarEventsDisplayedToUser, true);
}

}  // namespace calendar_metrics

}  // namespace ash
