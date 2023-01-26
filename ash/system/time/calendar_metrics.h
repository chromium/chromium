// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_METRICS_H_
#define ASH_SYSTEM_TIME_CALENDAR_METRICS_H_

#include <string>

namespace ui {
class AnimationThroughputReporter;
class Event;
}  // namespace ui

namespace base {
class TimeDelta;
}  // namespace base

namespace views {
class View;
}  // namespace views

namespace ash {

namespace calendar_metrics {

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

// The different hosts which hold components allowing a user to open the
// calendar. These are used in histograms, do not remove/renumber entries. If
// you're adding to this enum with the intention that it will be logged, update
// the CalendarViewShowSource token variant in histograms.xml.
enum class CalendarViewShowSource {
  kTimeView = 0,  // Shown via activating the time view in the status area.
  kDateView = 1,  // Shown via activating the  date view in the quick settings
                  // bubble.
  kAccelerator = 2,  // Shown via activating the accelerator.
  kMaxValue = kAccelerator
};

// Sources of scrolling inside the calendar view. These are used in histograms,
// do not remove/renumber entries. If you're adding to this enum with the
// intention that it will be logged, update the CalendarViewScrollSource token
// variant in enums.xml.
enum class CalendarViewScrollSource {
  kByMouseWheel = 0,
  kByGesture = 1,
  kByFling = 2,
  kByStylus = 3,
  kMaxValue = kByStylus
};

// The keys pressed by a user to navigate the calendar by using the keyboard.
// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// CalendarKeyboardNavigationSource enum in enums.xml.
enum class CalendarKeyboardNavigationSource {
  kTab = 0,
  kArrowKeys = 1,
  kMaxValue = kArrowKeys
};

// Converts the given event into an appropriate CalendarEventSource.
CalendarEventSource GetEventType(const ui::Event& event);

// Records calendar show metrics for a given CalendarViewShowSource
void RecordCalendarShowMetrics(
    CalendarViewShowSource show_source,
    calendar_metrics::CalendarEventSource event_source);

void RecordCalendarDateCellActivated(const ui::Event& event);

void RecordMonthArrowButtonActivated(bool up, const ui::Event& event);

void RecordEventListItemActivated(const ui::Event& event);

void RecordMonthDwellTime(const base::TimeDelta& dwell_time);

void RecordScrollSource(CalendarViewScrollSource source);

ui::AnimationThroughputReporter CreateAnimationReporter(
    views::View* view,
    const std::string& animation_histogram_name);

void RecordCalendarKeyboardNavigation(
    const CalendarKeyboardNavigationSource key_source);

void RecordEventListItemInUpNextLaunched(const ui::Event& event);

void RecordUpNextEventCount(const int event_count);

}  // namespace calendar_metrics

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_METRICS_H_
