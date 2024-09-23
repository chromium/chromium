// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_metrics.h"

#include "ash/public/cpp/metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
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
constexpr char kCalendarUserAction[] = "Ash.Calendar.UserAction";
constexpr char kCalendarWebUiOpened[] =
    "Ash.Calendar.UserActionToOpenCalendarWebUi";
constexpr char kCalendarFetchCalendarsFetchDuration[] =
    "Ash.Calendar.FetchCalendars.FetchDuration";
constexpr char kCalendarFetchCalendarsResult[] =
    "Ash.Calendar.FetchCalendars.Result";
constexpr char kCalendarFetchCalendarsTimeout[] =
    "Ash.Calendar.FetchCalendars.Timeout";
constexpr char kCalendarFetchEventsFetchDuration[] =
    "Ash.Calendar.FetchEvents.FetchDuration";
constexpr char kCalendarFetchEventsResult[] = "Ash.Calendar.FetchEvents.Result";
constexpr char kCalendarFetchEventsSingleMonthSize[] =
    "Ash.Calendar.FetchEvents.SingleMonthSize";
constexpr char kCalendarFetchEventsTimeout[] =
    "Ash.Calendar.FetchEvents.Timeout";
constexpr char kCalendarFetchEventsTotalCacheSizeMonths[] =
    "Ash.Calendar.FetchEvents.TotalCacheSizeMonths";
constexpr char kCalendarFetchEventsTotalFetchDuration[] =
    "Ash.Calendar.FetchEvents.TotalFetchDuration";
constexpr char kCalendarFetchCalendarsTotalSelectedCalendars[] =
    "Ash.Calendar.FetchCalendars.TotalSelectedCalendars";
constexpr char kCalendarTimeToSeeTodaysEventDots[] =
    "Ash.Calendar.TimeToSeeTodaysEventDots";
constexpr char kCalendarTimeToSeeTodaysEventDotsForMultiCalendar[] =
    "Ash.Calendar.TimeToSeeTodaysEventDotsForMultiCalendar";

// This enum is used in histograms. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused, only add
// at the end and. Also remember to update the CalendarUserAction enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class CalendarUserActionType {
  kDateCellActivated = 0,
  kMonthDownArrowActivated = 1,
  kMonthUpArrowActivated = 2,
  kScrolled = 3,
  kKeyboardNavigation = 4,
  kEventListItemPressed = 5,
  kUpNextItemPressed = 6,
  kEventListItemJoinButtonPressed = 7,
  kUpNextJoinButtonPressed = 8,
  kResetToTodayPressed = 9,
  kTodaysEventsInUpNextPressed = 10,
  kScrollInUpNext = 11,
  kCalendarLaunchedFromEmptyEventList = 12,
  kEventListClosed = 13,
  kSettingsButtonPressed = 14,
  kMaxValue = kSettingsButtonPressed,
};

bool ActionOpensCalendarWebUi(CalendarUserActionType action) {
  switch (action) {
    case CalendarUserActionType::kDateCellActivated:
    case CalendarUserActionType::kMonthDownArrowActivated:
    case CalendarUserActionType::kMonthUpArrowActivated:
    case CalendarUserActionType::kScrolled:
    case CalendarUserActionType::kKeyboardNavigation:
    case CalendarUserActionType::kResetToTodayPressed:
    case CalendarUserActionType::kTodaysEventsInUpNextPressed:
    case CalendarUserActionType::kScrollInUpNext:
    case CalendarUserActionType::kEventListClosed:
    case CalendarUserActionType::kSettingsButtonPressed:
      return false;
    case CalendarUserActionType::kEventListItemPressed:
    case CalendarUserActionType::kUpNextItemPressed:
    case CalendarUserActionType::kEventListItemJoinButtonPressed:
    case CalendarUserActionType::kUpNextJoinButtonPressed:
    case CalendarUserActionType::kCalendarLaunchedFromEmptyEventList:
      return true;
  }
}

void RecordCalendarUserAction(CalendarUserActionType action) {
  if (ActionOpensCalendarWebUi(action)) {
    base::UmaHistogramEnumeration(kCalendarWebUiOpened, action);
  }
  base::UmaHistogramEnumeration(kCalendarUserAction, action);
}

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
  RecordCalendarUserAction(CalendarUserActionType::kDateCellActivated);

  base::UmaHistogramEnumeration(kCalendarDateCellActivated,
                                GetEventType(event));
}

void RecordMonthArrowButtonActivated(bool up, const ui::Event& event) {
  RecordCalendarUserAction(
      up ? CalendarUserActionType::kMonthUpArrowActivated
         : CalendarUserActionType::kMonthDownArrowActivated);

  base::UmaHistogramEnumeration(up ? kCalendarMonthUpArrowButtonActivated
                                   : kCalendarMonthDownArrowButtonActivated,
                                GetEventType(event));
}

void RecordEventListItemActivated(const ui::Event& event) {
  RecordCalendarUserAction(CalendarUserActionType::kEventListItemPressed);

  base::UmaHistogramEnumeration(kCalendarEventListItemActivated,
                                GetEventType(event));
}

void RecordEventListForTodayActivated() {
  RecordCalendarUserAction(
      CalendarUserActionType::kTodaysEventsInUpNextPressed);
}

void RecordMonthDwellTime(const base::TimeDelta& dwell_time) {
  base::UmaHistogramMediumTimes(kCalendarMonthDwellTime, dwell_time);
}

void RecordResetToTodayPressed() {
  RecordCalendarUserAction(CalendarUserActionType::kResetToTodayPressed);
}

void RecordScrollSource(CalendarViewScrollSource source) {
  RecordCalendarUserAction(CalendarUserActionType::kScrolled);

  base::UmaHistogramEnumeration(kCalendarScrollSource, source);
}

ui::AnimationThroughputReporter CreateAnimationReporter(
    views::View* view,
    const std::string& animation_histogram_name) {
  // TODO(crbug.com/1297376): Add unit tests for animation metrics recording.
  return ui::AnimationThroughputReporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          [](const std::string& animation_histogram_name, int smoothness) {
            base::UmaHistogramPercentage(animation_histogram_name, smoothness);
          },
          animation_histogram_name)));
}

void RecordCalendarKeyboardNavigation(
    const CalendarKeyboardNavigationSource key_source) {
  RecordCalendarUserAction(CalendarUserActionType::kKeyboardNavigation);

  base::UmaHistogramEnumeration(kCalendarKeyboardNavigation, key_source);
}

void RecordEventListItemInUpNextLaunched(const ui::Event& event) {
  RecordCalendarUserAction(CalendarUserActionType::kUpNextItemPressed);

  base::UmaHistogramEnumeration(kCalendarEventListItemInUpNextPressed,
                                GetEventType(event));
}

void RecordUpNextEventCount(const int event_count) {
  base::UmaHistogramCounts100(kCalendarUpNextEventDisplayedCount, event_count);
}

void RecordJoinButtonPressedFromEventListView(const ui::Event& event) {
  RecordCalendarUserAction(
      CalendarUserActionType::kEventListItemJoinButtonPressed);

  base::UmaHistogramEnumeration(kCalendarEventListItemJoinButtonPressed,
                                GetEventType(event));
}

void RecordJoinButtonPressedFromUpNextView(const ui::Event& event) {
  RecordCalendarUserAction(CalendarUserActionType::kUpNextJoinButtonPressed);

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

void RecordScrollEventInUpNext() {
  RecordCalendarUserAction(CalendarUserActionType::kScrollInUpNext);
}

void RecordCalendarLaunchedFromEmptyEventList() {
  RecordCalendarUserAction(
      CalendarUserActionType::kCalendarLaunchedFromEmptyEventList);
}

void RecordEventListClosed() {
  RecordCalendarUserAction(CalendarUserActionType::kEventListClosed);
}

void RecordSettingsButtonPressed() {
  RecordCalendarUserAction(CalendarUserActionType::kSettingsButtonPressed);
}

void RecordCalendarListFetchDuration(const base::TimeDelta fetch_duration) {
  base::UmaHistogramTimes(kCalendarFetchCalendarsFetchDuration, fetch_duration);
}

void RecordCalendarListFetchErrorCode(google_apis::ApiErrorCode error) {
  base::UmaHistogramSparse(kCalendarFetchCalendarsResult, error);
}

void RecordCalendarListFetchTimeout(bool fetch_timed_out) {
  base::UmaHistogramBoolean(kCalendarFetchCalendarsTimeout, fetch_timed_out);
}

void RecordEventListFetchDuration(const base::TimeDelta fetch_duration) {
  base::UmaHistogramTimes(kCalendarFetchEventsFetchDuration, fetch_duration);
}

void RecordEventListFetchErrorCode(google_apis::ApiErrorCode error) {
  base::UmaHistogramSparse(kCalendarFetchEventsResult, error);
}

void RecordEventListFetchTimeout(bool fetch_timed_out) {
  base::UmaHistogramBoolean(kCalendarFetchEventsTimeout, fetch_timed_out);
}

void RecordEventListFetchesTotalDuration(const base::TimeDelta fetch_duration) {
  base::UmaHistogramTimes(kCalendarFetchEventsTotalFetchDuration,
                          fetch_duration);
}

void RecordSingleMonthSizeInBytes(size_t single_month_cache_size) {
  base::UmaHistogramCounts1M(kCalendarFetchEventsSingleMonthSize,
                             single_month_cache_size);
}

void RecordTotalEventsCacheSizeInMonths(unsigned int events_cache_size) {
  base::UmaHistogramCounts100000(kCalendarFetchEventsTotalCacheSizeMonths,
                                 events_cache_size);
}

void RecordTotalSelectedCalendars(unsigned int selected_calendars) {
  base::UmaHistogramCounts100000(kCalendarFetchCalendarsTotalSelectedCalendars,
                                 selected_calendars);
}

void RecordTimeToSeeTodaysEventDots(const base::TimeDelta time_elapsed,
                                    bool multi_calendar_enabled) {
  if (multi_calendar_enabled) {
    base::UmaHistogramMediumTimes(
        kCalendarTimeToSeeTodaysEventDotsForMultiCalendar, time_elapsed);
  } else {
    base::UmaHistogramMediumTimes(kCalendarTimeToSeeTodaysEventDots,
                                  time_elapsed);
  }
}

}  // namespace calendar_metrics

}  // namespace ash
