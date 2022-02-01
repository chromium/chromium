// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "ui/events/event.h"

namespace ash {

namespace calendar_metrics {

namespace {

// The event types CalendarView is interested in. These are used in histograms,
// do not remove/renumber entries. If you're adding to this enum with the
// intention that it will be logged, update the CalendarEventSource listing in
// enums.xml.
enum class CalendarEventSource {
  kInvalid = 0,
  kTap = 1,
  kClick = 2,
  kKeyboard = 3,
  kStylus = 4,
  kMaxValue = kStylus
};

constexpr char kCalendarViewShowSourcePrefix[] = "Ash.Calendar.ShowSource.";
constexpr char kCalendarDateCellActivated[] = "Ash.Calendar.DateCell.Activated";
constexpr char kCalendarEventListItemActivated[] =
    "Ash.Calendar.EventListItem.Activated";
constexpr char kCalendarMonthDownArrowButtonActivated[] =
    "Ash.Calendar.MonthDownArrowButton.Activated";
constexpr char kCalendarMonthUpArrowButtonActivated[] =
    "Ash.Calendar.MonthUpArrowButton.Activated";

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

}  // namespace

void RecordCalendarShowMetrics(CalendarViewShowSource show_source,
                               const ui::Event& event) {
  std::string histogram_name = kCalendarViewShowSourcePrefix;
  switch (show_source) {
    case CalendarViewShowSource::kDateView:
      histogram_name += "DateView";
      break;
    case CalendarViewShowSource::kTimeView:
      histogram_name += "TimeView";
      break;
    case CalendarViewShowSource::kAccelerator:
      DCHECK(event.IsKeyEvent());
      histogram_name += "Keyboard";
      break;
  }

  base::UmaHistogramEnumeration(histogram_name, GetEventType(event));
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

}  // namespace calendar_metrics

}  // namespace ash
