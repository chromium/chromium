// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/events/event.h"

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
  UMA_HISTOGRAM_TIMES(kCalendarMonthDwellTime, dwell_time);
}

void RecordScrollSource(CalendarViewScrollSource source) {
  base::UmaHistogramEnumeration(kCalendarScrollSource, source);
}

}  // namespace calendar_metrics

}  // namespace ash
