// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/time/date_helper.h"
#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

namespace {

// The thickness of the border.
constexpr int kBorderLineThickness = 2;

// The radius used to draw the border.
constexpr float kBorderRadius = 21.f;
constexpr float kBorderRadiusGlanceables = 19.f;

// The radius used to draw "today's" date cell view border and background.
constexpr float kTodayBorderRadius = 100.f;

// The insets used to draw "today's" date cell view.
constexpr float kTodayRoundedBackgroundHorizontalInset = 8.f;
constexpr float kTodayRoundedBackgroundVerticalInset = 0.f;
constexpr float kTodayRoundedBackgroundHorizontalFocusedInset =
    kTodayRoundedBackgroundHorizontalInset + kBorderLineThickness + 2.f;
constexpr float kTodayRoundedBackgroundVerticalFocusedInset =
    kTodayRoundedBackgroundVerticalInset + kBorderLineThickness + 2.f;

// For Glanceables: The horizontal inset used to draw "today's" date cell view.
constexpr float kTodayRoundedBackgroundHorizontalInsetGlanceables = 9.f;

// Radius of the small dot displayed on a CalendarDateCellView if events are
// present for that day.
constexpr float kEventsPresentRoundedRadius = 1.f;

// The gap padding between the date and the indicator.
constexpr int kGapBetweenDateAndIndicator = 1;

// The insets within the date cell.
constexpr int kDateCellVerticalPaddingGlanceables = 10;
constexpr auto kDateCellInsetsGlanceables =
    gfx::Insets::VH(kDateCellVerticalPaddingGlanceables, 16);

// Move to the next day. Both the column and the current date are moved to the
// next one.
void MoveToNextDay(int& column,
                   base::Time& current_date,
                   base::Time& local_current_date,
                   base::Time::Exploded& current_date_exploded) {
  // Using 30 hours to make sure the date is moved to the next day, since there
  // are daylight saving days which have more than 24 hours in a day.
  // `base::Days(1)` cannot be used, because it is 24 hours.
  //
  // Also using the `current_date_exploded` hours to calculate the local
  // midnight.
  int hours = current_date_exploded.hour;
  current_date += base::Hours(30 - hours);
  local_current_date += base::Hours(30 - hours);
  local_current_date.UTCExplode(&current_date_exploded);
  column = (column + 1) % calendar_utils::kDateInOneWeek;
}

}  // namespace

CalendarDateCellView::CalendarDateCellView(
    CalendarViewController* calendar_view_controller,
    base::Time date,
    base::TimeDelta time_difference,
    bool is_grayed_out_date,
    bool should_fetch_calendar_data,
    int row_index,
    bool is_fetched)
    : views::LabelButton(
          views::Button::PressedCallback(
              base::BindRepeating(&CalendarDateCellView::OnDateCellActivated,
                                  base::Unretained(this))),
          calendar_utils::GetDayIntOfMonth(date + time_difference),
          CONTEXT_CALENDAR_DATE),
      date_(date),
      grayed_out_(is_grayed_out_date),
      should_fetch_calendar_data_(should_fetch_calendar_data),
      row_index_(row_index),
      is_fetched_(is_fetched),
      is_today_(calendar_utils::IsToday(date)),
      time_difference_(time_difference),
      calendar_view_controller_(calendar_view_controller) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(
      features::AreAnyGlanceablesTimeManagementViewsEnabled()
          ? kDateCellInsetsGlanceables
          : calendar_utils::kDateCellInsets));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);
  if (is_today_) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                          *label());
  } else {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody1,
                                          *label());
  }
  views::FocusRing::Remove(this);

  DisableFocus();
  if (!grayed_out_) {
    if (should_fetch_calendar_data_ && is_fetched_) {
      UpdateFetchStatus(true);
    }

    SetTooltipAndAccessibleName();
    is_selected_ = calendar_view_controller->selected_date_cell_view() == this;
  }
  scoped_calendar_view_controller_observer_.Observe(
      calendar_view_controller_.get());
}

CalendarDateCellView::~CalendarDateCellView() = default;

void CalendarDateCellView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Gray-out the date that is not in the current month.
  SetEnabledTextColorIds(grayed_out_ ? cros_tokens::kCrosSysOnSurfaceVariant
                                     : cros_tokens::kCrosSysOnSurface);
}

// Draws the background for this date. Note that this includes not only the
// circular fill (if any), but also the border (if focused) and text color. If
// this is a grayed out date, which is shown in its previous/next month, this
// background won't be drawn.
void CalendarDateCellView::OnPaintBackground(gfx::Canvas* canvas) {
  if (grayed_out_) {
    return;
  }

  const gfx::Rect content = GetContentsBounds();
  const gfx::SizeF local_bounds = gfx::SizeF(GetLocalBounds().size());

  const SkColor border_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing);
  cc::PaintFlags highlight_border;
  highlight_border.setColor(border_color);
  highlight_border.setAntiAlias(true);
  highlight_border.setStyle(cc::PaintFlags::kStroke_Style);
  highlight_border.setStrokeWidth(kBorderLineThickness);

  const bool is_for_glanceables =
      features::AreAnyGlanceablesTimeManagementViewsEnabled();
  if (is_today_) {
    gfx::RectF background_rect(local_bounds);

    const SkColor bg_color = GetColorProvider()->GetColor(
        cros_tokens::kCrosSysSystemPrimaryContainer);
    cc::PaintFlags highlight_background;
    highlight_background.setColor(bg_color);
    highlight_background.setStyle(cc::PaintFlags::kFill_Style);
    highlight_background.setAntiAlias(true);

    // If the today view is focused, we draw a border around the background
    // and inset the background a couple of pixels to leave 2dp of space
    // between.
    // Else we just draw the background full size with no border.
    if (views::View::HasFocus()) {
      gfx::RectF border_rect(local_bounds);
      const int half_stroke_thickness = kBorderLineThickness / 2;
      border_rect.Inset(gfx::InsetsF::VH(
          half_stroke_thickness,
          is_for_glanceables ? kTodayRoundedBackgroundHorizontalInsetGlanceables
                             : kTodayRoundedBackgroundHorizontalInset));
      canvas->DrawRoundRect(border_rect, kTodayBorderRadius, highlight_border);

      background_rect.Inset(
          gfx::InsetsF::VH(kTodayRoundedBackgroundVerticalFocusedInset,
                           kTodayRoundedBackgroundHorizontalFocusedInset));
      canvas->DrawRoundRect(background_rect, kTodayBorderRadius,
                            highlight_background);

      return;
    }

    background_rect.Inset(
        gfx::InsetsF::VH(kTodayRoundedBackgroundVerticalInset,
                         kTodayRoundedBackgroundHorizontalInset));
    canvas->DrawRoundRect(background_rect, kTodayBorderRadius,
                          highlight_background);

    return;
  }

  // If !today and view is focused or selected, draw a circle around the view.
  if (views::View::HasFocus() || is_selected_) {
    const gfx::Point center(
        (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
        (content.height() + (is_for_glanceables
                                 ? kDateCellVerticalPaddingGlanceables
                                 : calendar_utils::kDateVerticalPadding) *
                                2) /
            2);
    canvas->DrawCircle(
        center, is_for_glanceables ? kBorderRadiusGlanceables : kBorderRadius,
        highlight_border);
  }
}

void CalendarDateCellView::OnSelectedDateUpdated() {
  const bool is_selected =
      calendar_view_controller_->selected_date_cell_view() == this;
  // If the selected day changes, repaint the background.
  if (is_selected_ != is_selected) {
    is_selected_ = is_selected;
    SchedulePaint();
    if (!is_selected_) {
      GetViewAccessibility().SetName(tool_tip_);
      return;
    }
    // Sets accessible label. E.g. Calendar, week of July 16th 2021, [selected
    // date] is currently selected.
    base::Time local_date = date_ + time_difference_;
    base::Time::Exploded date_exploded =
        calendar_utils::GetExplodedUTC(local_date);
    base::Time first_day_of_week =
        date_ - base::Days(date_exploded.day_of_week);

    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_CALENDAR_SELECTED_DATE_CELL_ACCESSIBLE_DESCRIPTION,
        calendar_utils::GetMonthDayYear(first_day_of_week),
        calendar_utils::GetDayOfMonth(date_)));
  }
}

void CalendarDateCellView::CloseEventList() {
  if (!is_selected_) {
    return;
  }

  // If this date is selected, repaint the background.
  is_selected_ = false;
  SchedulePaint();
}

void CalendarDateCellView::EnableFocus() {
  if (grayed_out_) {
    return;
  }
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarDateCellView::DisableFocus() {
  SetFocusBehavior(FocusBehavior::NEVER);
}

void CalendarDateCellView::SetTooltipAndAccessibleName() {
  std::u16string formatted_date = calendar_utils::GetMonthDayYearWeek(date_);
  if (!should_fetch_calendar_data_) {
    tool_tip_ = formatted_date;
  } else {
    if (is_fetched_) {
      const int tooltip_id =
          event_number_ == 1 ? IDS_ASH_CALENDAR_DATE_CELL_TOOLTIP
                             : IDS_ASH_CALENDAR_DATE_CELL_PLURAL_EVENTS_TOOLTIP;
      tool_tip_ = l10n_util::GetStringFUTF16(
          tooltip_id, formatted_date,
          base::UTF8ToUTF16(base::NumberToString(event_number_)));
    } else {
      const int tooltip_id = IDS_ASH_CALENDAR_DATE_CELL_LOADING_TOOLTIP;
      tool_tip_ = l10n_util::GetStringFUTF16(tooltip_id, formatted_date);
    }
  }
  SetTooltipText(tool_tip_);
  GetViewAccessibility().SetName(tool_tip_);
}

void CalendarDateCellView::UpdateFetchStatus(bool is_fetched) {
  // No need to re-paint the grayed out cells, since here should be no change
  // for them.
  if (grayed_out_) {
    return;
  }

  if (!should_fetch_calendar_data_) {
    SetTooltipAndAccessibleName();
    return;
  }

  // If the fetching status remains unfetched, no need to schedule repaint.
  if (!is_fetched_ && !is_fetched) {
    return;
  }

  // If the events are fetched, gets the event number and checks if the event
  // number has been changed. If the event number hasn't been changed and the
  // events have been fetched before (i.e. a re-fetch with no event number
  // change), no need to repaint. In all other cases, schedules a repaint.
  if (is_fetched) {
    const int event_number = calendar_view_controller_->GetEventNumber(date_);
    if (event_number_ == event_number && is_fetched_) {
      return;
    }

    event_number_ = event_number;
    if (is_today_) {
      calendar_view_controller_->OnTodaysEventFetchComplete();
    }
  }

  is_fetched_ = is_fetched;
  SetTooltipAndAccessibleName();
  SchedulePaint();
}

void CalendarDateCellView::SetFirstOnFocusedAccessibilityLabel() {
  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_DATE_CELL_ON_FOCUS_ACCESSIBLE_DESCRIPTION, tool_tip_));
}

void CalendarDateCellView::PaintButtonContents(gfx::Canvas* canvas) {
  views::LabelButton::PaintButtonContents(canvas);
  if (grayed_out_) {
    return;
  }

  SetEnabledTextColorIds(is_today_
                             ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                             : cros_tokens::kCrosSysOnSurface);
  MaybeDrawEventsIndicator(canvas);
}

void CalendarDateCellView::OnDateCellActivated(const ui::Event& event) {
  if (grayed_out_ || !should_fetch_calendar_data_ ||
      !calendar_view_controller_->is_date_cell_clickable()) {
    return;
  }

  // Explicitly request focus after being activated to ensure focus moves away
  // from any CalendarDateCellView which was focused prior.
  RequestFocus();
  calendar_metrics::RecordCalendarDateCellActivated(event);
  calendar_view_controller_->ShowEventListView(/*selected_date_cell_view=*/this,
                                               date_, row_index_);
}

gfx::Point CalendarDateCellView::GetEventsPresentIndicatorCenterPosition() {
  const gfx::Rect content = GetContentsBounds();
  const int horizontal_padding = calendar_utils::kDateHorizontalPadding;
  const int vertical_padding =
      features::AreAnyGlanceablesTimeManagementViewsEnabled()
          ? kDateCellVerticalPaddingGlanceables
          : calendar_utils::kDateVerticalPadding;
  return gfx::Point(
      (content.width() + horizontal_padding * 2) / 2,
      content.height() + vertical_padding + kGapBetweenDateAndIndicator);
}

void CalendarDateCellView::MaybeDrawEventsIndicator(gfx::Canvas* canvas) {
  // Not drawing the event dot if it's a grayed out cell or the user is not in
  // an active session (without a vilid user account id).
  if (grayed_out_ || !should_fetch_calendar_data_) {
    return;
  }

  if (event_number_ == 0) {
    return;
  }

  const auto* color_provider = GetColorProvider();
  const SkColor indicator_color = color_provider->GetColor(
      is_today_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                : cros_tokens::kCrosSysOnSurface);

  const float indicator_radius = is_selected_ ? kEventsPresentRoundedRadius * 2
                                              : kEventsPresentRoundedRadius;

  cc::PaintFlags indicator_paint_flags;
  indicator_paint_flags.setColor(indicator_color);
  indicator_paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  indicator_paint_flags.setAntiAlias(true);
  canvas->DrawCircle(GetEventsPresentIndicatorCenterPosition(),
                     indicator_radius, indicator_paint_flags);
  is_events_indicator_drawn = true;
}

CalendarMonthView::CalendarMonthView(
    const base::Time first_day_of_month,
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      calendar_list_model_(
          Shell::Get()->system_tray_model()->calendar_list_model()),
      calendar_model_(Shell::Get()->system_tray_model()->calendar_model()) {
  views::TableLayout* layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  // This layer is required for animations.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  calendar_utils::SetUpWeekColumns(layout);
  base::TimeDelta const time_difference =
      calendar_utils::GetTimeDifference(first_day_of_month);

  // Using the time difference to get the local `base::Time`, which is used to
  // generate the exploded.
  base::Time first_day_of_month_local = first_day_of_month + time_difference;
  base::Time::Exploded first_day_of_month_exploded =
      calendar_utils::GetExplodedUTC(first_day_of_month_local);
  // Find the first day of the week. Use 8:00 in the morning to avoid any issues
  // caused by DTS, since some timezones' DST start at midnight, some start at
  // 1:00AM etc, but no one starts at 8:00 in the morning.
  base::Time current_date =
      calendar_utils::GetFirstDayOfWeekLocalMidnight(first_day_of_month) +
      base::Hours(8);
  base::Time current_date_local = current_date + time_difference;
  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExplodedUTC(current_date_local);

  fetch_month_ = first_day_of_month_local.UTCMidnight();

  if (calendar_utils::IsMultiCalendarEnabled()) {
    // Set up the Calendar List Model observer to trigger an event fetch only
    // after the calendar list fetch is completed.
    scoped_calendar_list_model_observer_.Observe(calendar_list_model_.get());

    // If the month view has been created after a successful calendar list
    // fetch, this will trigger an event list fetch immediately. Otherwise,
    // events will be fetched during `OnCalendarListFetchComplete`.
    calendar_model_->MaybeFetchEvents(fetch_month_);
  } else {
    FetchEvents(fetch_month_);
  }

  bool has_fetched_data =
      calendar_view_controller_->IsSuccessfullyFetched(fetch_month_);
  const bool should_fetch_calendar_data =
      calendar_utils::ShouldFetchCalendarData();

  // TODO(crbug.com/40192677): Extract the following 3 parts (while
  // loops) into a method.
  int column = 0;
  int safe_index = 0;
  // Gray-out dates in the first row, which are from the previous month.
  while (current_date_exploded.month % 12 ==
         (first_day_of_month_exploded.month - 1) % 12) {
    AddDateCellToLayout(current_date, column,
                        /*is_in_current_month=*/false, /*row_index=*/0,
                        /*is_fetched=*/has_fetched_data,
                        should_fetch_calendar_data);
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);
    ++safe_index;
    if (safe_index == calendar_utils::kDateInOneWeek) {
      DUMP_WILL_BE_NOTREACHED()
          << "Should not render more than 7 days as the grayed out cells.";
      break;
    }
  }

  int row_number = 0;
  safe_index = 0;
  // Builds non-gray-out dates of the current month.
  while (current_date_exploded.month == first_day_of_month_exploded.month) {
    // Count a row when a new row starts.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      ++row_number;
    }
    auto* cell = AddDateCellToLayout(current_date, column,
                                     /*is_in_current_month=*/true,
                                     /*row_index=*/row_number - 1,
                                     /*is_fetched=*/has_fetched_data,
                                     should_fetch_calendar_data);
    // Add the first non-grayed-out cell of the row to the `focused_cells_`.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      focused_cells_.push_back(cell);
    }
    // If this row has today, updates today's row number and replaces today to
    // the last element in the `focused_cells_`.
    if (cell->is_today()) {
      calendar_view_controller_->set_row_height(
          cell->GetPreferredSize().height());
      calendar_view_controller_->set_today_row(row_number);
      focused_cells_.back() = cell;
      has_today_ = true;
      DCHECK(calendar_view_controller_->todays_date_cell_view() == nullptr);
      calendar_view_controller_->set_todays_date_cell_view(cell);
    }
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);

    ++safe_index;
    if (safe_index == 32) {
      NOTREACHED() << "Should not render more than 31 days in a month.";
    }
  }

  last_row_index_ = row_number - 1;

  // To receive the fetched events.
  scoped_calendar_model_observer_.Observe(calendar_model_.get());

  // Gets the fetched status again in case the events are fetched in the middle
  // of rendering date cells.
  bool updated_has_fetched_data =
      calendar_view_controller_->IsSuccessfullyFetched(fetch_month_);

  // If the fetching status changed, schedule repaint.
  if (updated_has_fetched_data != has_fetched_data) {
    UpdateIsFetchedAndRepaint(updated_has_fetched_data);
  }

  if (calendar_utils::GetDayOfWeekInt(current_date) == 1) {
    return;
  }

  // Adds the first several days from the next month if the last day is not the
  // end day of this week. The end date of the last row should be 6 day's away
  // from the first day of this week. Adds `kDurationForAdjustingDST` hours to
  // cover the case 25 hours in a day due to daylight saving.
  base::Time end_of_the_last_row_local =
      calendar_utils::GetFirstDayOfWeekLocalMidnight(current_date) +
      base::Days(6) + calendar_utils::kDurationForAdjustingDST +
      time_difference;
  base::Time::Exploded end_of_row_exploded =
      calendar_utils::GetExplodedUTC(end_of_the_last_row_local);

  safe_index = 0;
  // Gray-out dates in the last row, which are from the next month.
  while (current_date_exploded.day_of_month <=
         end_of_row_exploded.day_of_month) {
    // Next column is generated.
    AddDateCellToLayout(current_date, column,
                        /*is_in_current_month=*/false,
                        /*row_index=*/row_number,
                        /*is_fetched=*/has_fetched_data,
                        should_fetch_calendar_data);
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);

    ++safe_index;
    if (safe_index == calendar_utils::kDateInOneWeek) {
      // "CMV" stands for `CalendarMonthView`, the printed log should be like:
      // CMV-locale  ru
      // CMV-timezone America/Los_Angeles
      // CMV-now_date_local 13 марта 2024 г.
      // CMV-now_time_local 18:04
      // CMV-week_header ПВСЧПСВ
      // CMV-last_day_of_last_row  20240303
      // CMV-last_day_of_last_row  500
      // CMV-first_day_of_month 20240201
      // CMV-first_day_of_month_time 1704
      SCOPED_CRASH_KEY_STRING32("CMV", "locale",
                                base::i18n::GetConfiguredLocale());
      SCOPED_CRASH_KEY_STRING32(
          "CMV", "time_zone",
          base::UTF16ToUTF8(
              system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID()));
      SCOPED_CRASH_KEY_STRING32(
          "CMV", "now_date_local",
          base::UTF16ToUTF8(
              calendar_utils::GetMonthDayYear(base::Time::Now())));
      SCOPED_CRASH_KEY_STRING32(
          "CMV", "now_time_local",
          base::UTF16ToUTF8(
              calendar_utils::GetTwentyFourHourClockTime(base::Time::Now())));
      std::u16string week = u"";
      for (const std::u16string& day :
           DateHelper::GetInstance()->week_titles()) {
        week += day;
      }
      SCOPED_CRASH_KEY_STRING32("CMV", "week_header", base::UTF16ToUTF8(week));
      SCOPED_CRASH_KEY_NUMBER("CMV", "last_day_of_last_row",
                              10000 * end_of_row_exploded.year +
                                  100 * end_of_row_exploded.month +
                                  end_of_row_exploded.day_of_month);
      SCOPED_CRASH_KEY_NUMBER(
          "CMV", "last_day_of_last_row_time",
          100 * end_of_row_exploded.hour + end_of_row_exploded.minute);
      SCOPED_CRASH_KEY_NUMBER("CMV", "first_day_of_month",
                              10000 * first_day_of_month_exploded.year +
                                  100 * first_day_of_month_exploded.month +
                                  first_day_of_month_exploded.day_of_month);
      SCOPED_CRASH_KEY_NUMBER("CMV", "first_day_of_month_time",
                              100 * first_day_of_month_exploded.hour +
                                  first_day_of_month_exploded.minute);
      NOTREACHED()
          << "Should not render more than 7 days as the gray out cells.";
    }
  }
}

CalendarMonthView::~CalendarMonthView() {
  calendar_model_->CancelFetch(fetch_month_);

  auto* todays_date_cell_view =
      calendar_view_controller_->todays_date_cell_view();
  if (todays_date_cell_view && todays_date_cell_view->parent() == this) {
    calendar_view_controller_->set_todays_date_cell_view(nullptr);
  }

  auto* selected_date_cell_view =
      calendar_view_controller_->selected_date_cell_view();
  if (selected_date_cell_view && selected_date_cell_view->parent() == this) {
    calendar_view_controller_->set_selected_date_cell_view(nullptr);
  }
}

void CalendarMonthView::OnCalendarListFetchComplete() {
  // When the Calendar gets opened and the first 5 month views are created,
  // the calendar list is usually not yet ready when FetchEvents gets called in
  // the constructor.
  // Therefore, the first month views call FetchEvents when the calendar list
  // model signals that the fetch is complete. Any month views created after
  // the calendar list is ready will fetch events immediately during the
  // constructor instead.
  if (calendar_utils::IsMultiCalendarEnabled()) {
    calendar_model_->FetchEvents(fetch_month_);
  }
}

void CalendarMonthView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time) {
  if (status == CalendarModel::kSuccess && start_time == fetch_month_) {
    UpdateIsFetchedAndRepaint(true);
  }

  if (!(calendar_model_->MonthHasEvents(start_time))) {
    return;
  }

  has_events_ = true;

  if (start_time ==
      calendar_view_controller_->GetOnScreenMonthFirstDayUTC().UTCMidnight()) {
    calendar_view_controller_->EventsDisplayedToUser();
  }
}

void CalendarMonthView::EnableFocus() {
  for (views::View* cell : children()) {
    static_cast<CalendarDateCellView*>(cell)->EnableFocus();
  }
}

void CalendarMonthView::DisableFocus() {
  for (views::View* cell : children()) {
    static_cast<CalendarDateCellView*>(cell)->DisableFocus();
  }
}

void CalendarMonthView::UpdateIsFetchedAndRepaint(bool updated_is_fetched) {
  for (views::View* cell : children()) {
    static_cast<CalendarDateCellView*>(cell)->UpdateFetchStatus(
        updated_is_fetched);
  }
}

BEGIN_METADATA(CalendarMonthView)
END_METADATA

CalendarDateCellView* CalendarMonthView::AddDateCellToLayout(
    base::Time current_date,
    int column,
    bool is_in_current_month,
    int row_index,
    bool is_fetched,
    bool should_fetch_calendar_data) {
  auto* layout_manager = static_cast<views::TableLayout*>(GetLayoutManager());
  if (column == 0) {
    layout_manager->AddRows(1, views::TableLayout::kFixedSize);
  }
  return AddChildView(std::make_unique<CalendarDateCellView>(
      calendar_view_controller_, current_date,
      calendar_utils::GetTimeDifference(current_date),
      /*is_grayed_out_date=*/!is_in_current_month, should_fetch_calendar_data,
      /*row_index=*/row_index,
      /*is_fetched=*/is_fetched));
}

void CalendarMonthView::FetchEvents(const base::Time& month) {
  calendar_model_->FetchEvents(month);
}

BEGIN_METADATA(CalendarDateCellView)
END_METADATA

}  // namespace ash
