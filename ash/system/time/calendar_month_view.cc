// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

namespace {

// The thickness of the border.
constexpr int kBorderLineThickness = 2;

// The radius used to draw the border.
constexpr float kBorderRadius = 21.f;

// The default radius used to draw rounded today's circle.
constexpr float kTodayRoundedRadius = 22.f;

// The radius used to draw rounded today's circle when focused.
constexpr float kTodayFocusedRoundedRadius = 18.f;

// Radius of the small dot displayed on a CalendarDateCellView if events are
// present for that day.
constexpr float kEventsPresentRoundedRadius = 1.f;

// The gap padding between the date and the indicator.
constexpr int kGapBetweenDateAndIndicator = 1;

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
    bool is_grayed_out_date,
    int row_index)
    : views::LabelButton(
          views::Button::PressedCallback(
              base::BindRepeating(&CalendarDateCellView::OnDateCellActivated,
                                  base::Unretained(this))),
          calendar_utils::GetDayIntOfMonth(
              date + base::Minutes(
                         calendar_view_controller->time_difference_minutes())),
          CONTEXT_CALENDAR_DATE),
      date_(date),
      grayed_out_(is_grayed_out_date),
      row_index_(row_index),
      calendar_view_controller_(calendar_view_controller) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(calendar_utils::kDateCellInsets));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);

  views::FocusRing::Remove(this);

  DisableFocus();
  if (!grayed_out_) {
    if (calendar_utils::IsActiveUser()) {
      event_number_ = calendar_view_controller_->GetEventNumber(date_);
    }
    SetTooltipAndAccessibleName();
  }
  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
}

CalendarDateCellView::~CalendarDateCellView() = default;

void CalendarDateCellView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Gray-out the date that is not in the current month.
  SetEnabledTextColors(grayed_out_ ? calendar_utils::GetSecondaryTextColor()
                                   : calendar_utils::GetPrimaryTextColor());
}

// Draws the background for this date. Note that this includes not only the
// circular fill (if any), but also the border (if focused) and text color. If
// this is a grayed out date, which is shown in its previous/next month, this
// background won't be drawn.
void CalendarDateCellView::OnPaintBackground(gfx::Canvas* canvas) {
  if (grayed_out_)
    return;

  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor bg_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
  const SkColor border_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  const gfx::Rect content = GetContentsBounds();
  const gfx::Point center(
      (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
      (content.height() + calendar_utils::kDateVerticalPadding * 2) / 2);

  if (views::View::HasFocus()) {
    cc::PaintFlags highlight_border;
    highlight_border.setColor(border_color);
    highlight_border.setAntiAlias(true);
    highlight_border.setStyle(cc::PaintFlags::kStroke_Style);
    highlight_border.setStrokeWidth(kBorderLineThickness);

    canvas->DrawCircle(center, kBorderRadius, highlight_border);
  }

  if (calendar_utils::IsToday(date_)) {
    cc::PaintFlags highlight_background;
    highlight_background.setColor(bg_color);
    highlight_background.setStyle(cc::PaintFlags::kFill_Style);
    highlight_background.setAntiAlias(true);

    canvas->DrawCircle(center,
                       views::View::HasFocus() ? kTodayFocusedRoundedRadius
                                               : kTodayRoundedRadius,
                       highlight_background);
  }
}

void CalendarDateCellView::OnSelectedDateUpdated() {
  bool is_selected = calendar_utils::IsTheSameDay(
      date_, calendar_view_controller_->selected_date());
  // If the selected day changes, repaint the background.
  if (is_selected_ != is_selected) {
    is_selected_ = is_selected;
    SchedulePaint();
    if (!is_selected_) {
      SetAccessibleName(tool_tip_);
      return;
    }
    // Sets accessible label. E.g. Calendar, week of July 16th 2021, [selected
    // date] is currently selected.
    base::Time local_date =
        date_ +
        base::Minutes(calendar_view_controller_->time_difference_minutes());
    base::Time::Exploded date_exploded =
        calendar_utils::GetExplodedUTC(local_date);
    base::Time first_day_of_week =
        date_ - base::Days(date_exploded.day_of_week);

    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_CALENDAR_SELECTED_DATE_CELL_ACCESSIBLE_DESCRIPTION,
        calendar_utils::GetMonthDayYear(first_day_of_week),
        calendar_utils::GetDayOfMonth(date_)));
  }
}

void CalendarDateCellView::CloseEventList() {
  if (!is_selected_)
    return;

  // If this date is selected, repaint the background.
  is_selected_ = false;
  SchedulePaint();
}

void CalendarDateCellView::EnableFocus() {
  if (grayed_out_)
    return;
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarDateCellView::DisableFocus() {
  SetFocusBehavior(FocusBehavior::NEVER);
}

void CalendarDateCellView::SetTooltipAndAccessibleName() {
  std::u16string formatted_date = calendar_utils::GetMonthDayYear(date_);
  if (!calendar_utils::IsActiveUser()) {
    tool_tip_ = formatted_date;
  } else {
    const int tooltip_id =
        event_number_ == 1 ? IDS_ASH_CALENDAR_DATE_CELL_TOOLTIP
                           : IDS_ASH_CALENDAR_DATE_CELL_PLURAL_EVENTS_TOOLTIP;
    tool_tip_ = l10n_util::GetStringFUTF16(
        tooltip_id, formatted_date,
        base::UTF8ToUTF16(base::NumberToString(event_number_)));
  }
  SetTooltipText(tool_tip_);
  SetAccessibleName(tool_tip_);
}

void CalendarDateCellView::MaybeSchedulePaint() {
  // No need to re-paint the grayed out cells, since here should be no change
  // for them.
  if (grayed_out_)
    return;

  if (!calendar_utils::IsActiveUser()) {
    SetTooltipAndAccessibleName();
    return;
  }

  // Early return if the event number doesn't change.
  const int event_number = calendar_view_controller_->GetEventNumber(date_);
  if (event_number_ == event_number)
    return;

  event_number_ = event_number;
  SetTooltipAndAccessibleName();
  SchedulePaint();
}

void CalendarDateCellView::SetFirstOnFocusedAccessibilityLabel() {
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_DATE_CELL_ON_FOCUS_ACCESSIBLE_DESCRIPTION, tool_tip_));
}

gfx::Point CalendarDateCellView::GetEventsPresentIndicatorCenterPosition() {
  const gfx::Rect content = GetContentsBounds();
  return gfx::Point(
      (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
      content.height() + calendar_utils::kDateVerticalPadding +
          kGapBetweenDateAndIndicator);
}

void CalendarDateCellView::MaybeDrawEventsIndicator(gfx::Canvas* canvas) {
  // Not drawing the event dot if it's a grayed out cell or the user is not in
  // an active session (without a vilid user account id).
  if (grayed_out_ || !calendar_utils::IsActiveUser())
    return;

  if (event_number_ == 0)
    return;

  const SkColor indicator_color =
      calendar_utils::IsToday(date_)
          ? AshColorProvider::Get()->GetBaseLayerColor(
                AshColorProvider::BaseLayerType::kTransparent90)
          : AshColorProvider::Get()->GetControlsLayerColor(
                AshColorProvider::ControlsLayerType::kFocusRingColor);

  const float indicator_radius = is_selected_ ? kEventsPresentRoundedRadius * 2
                                              : kEventsPresentRoundedRadius;

  cc::PaintFlags indicator_paint_flags;
  indicator_paint_flags.setColor(indicator_color);
  indicator_paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  indicator_paint_flags.setAntiAlias(true);
  canvas->DrawCircle(GetEventsPresentIndicatorCenterPosition(),
                     indicator_radius, indicator_paint_flags);
}

void CalendarDateCellView::PaintButtonContents(gfx::Canvas* canvas) {
  views::LabelButton::PaintButtonContents(canvas);
  if (grayed_out_)
    return;

  const AshColorProvider* color_provider = AshColorProvider::Get();
  if (calendar_utils::IsToday(date_)) {
    const SkColor text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonLabelColorPrimary);
    SetEnabledTextColors(text_color);
  } else if (is_selected_) {
    const SkColor text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorProminent);
    SetEnabledTextColors(text_color);
  } else {
    SetEnabledTextColors(grayed_out_ ? calendar_utils::GetSecondaryTextColor()
                                     : calendar_utils::GetPrimaryTextColor());
  }
  MaybeDrawEventsIndicator(canvas);
}

void CalendarDateCellView::OnDateCellActivated(const ui::Event& event) {
  if (grayed_out_ || !calendar_utils::IsActiveUser())
    return;

  // Explicitly request focus after being activated to ensure focus moves away
  // from any CalendarDateCellView which was focused prior.
  RequestFocus();
  calendar_metrics::RecordCalendarDateCellActivated(event);
  calendar_view_controller_->ShowEventListView(date_, row_index_);
}

CalendarMonthView::CalendarMonthView(
    const base::Time first_day_of_month,
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      calendar_model_(Shell::Get()->system_tray_model()->calendar_model()) {
  views::TableLayout* layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  // The layer is required in animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  calendar_utils::SetUpWeekColumns(layout);
  calendar_view_controller_->MaybeUpdateTimeDifference(first_day_of_month);

  // Using the time difference to get the local `base::Time`, which is used to
  // generate the exploded.
  base::Time first_day_of_month_local =
      first_day_of_month +
      base::Minutes(calendar_view_controller_->time_difference_minutes());
  base::Time::Exploded first_day_of_month_exploded =
      calendar_utils::GetExplodedUTC(first_day_of_month_local);
  // Find the first day of the week.
  base::Time current_date =
      calendar_utils::GetFirstDayOfWeekLocalMidnight(first_day_of_month);
  base::Time current_date_local =
      current_date +
      base::Minutes(calendar_view_controller_->time_difference_minutes());

  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExplodedUTC(current_date_local);

  // Fetch events for the month.
  fetch_month_ = first_day_of_month_local.UTCMidnight();
  FetchEvents(fetch_month_);

  // TODO(https://crbug.com/1236276): Extract the following 3 parts (while
  // loops) into a method.
  int column = 0;
  // Gray-out dates in the first row, which are from the previous month.
  while (current_date_exploded.month % 12 ==
         (first_day_of_month_exploded.month - 1) % 12) {
    AddDateCellToLayout(current_date, column,
                        /*is_in_current_month=*/false, /*row_index=*/0);
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);
  }

  int row_number = 0;
  // Builds non-gray-out dates of the current month.
  while (current_date_exploded.month == first_day_of_month_exploded.month) {
    // Count a row when a new row starts.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      ++row_number;
    }
    auto* cell = AddDateCellToLayout(current_date, column,
                                     /*is_in_current_month=*/true,
                                     /*row_index=*/row_number - 1);
    // Add the first non-grayed-out cell of the row to the `focused_cells_`.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      focused_cells_.push_back(cell);
    }
    // If this row has today, updates today's row number and replaces today to
    // the last element in the `focused_cells_`.
    if (calendar_utils::IsToday(current_date)) {
      calendar_view_controller_->set_row_height(
          cell->GetPreferredSize().height());
      calendar_view_controller_->set_today_row(row_number);
      focused_cells_.back() = cell;
      has_today_ = true;
    }
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);
  }

  last_row_index_ = row_number - 1;

  // To receive the fetched events.
  scoped_calendar_model_observer_.Observe(calendar_model_);

  if (calendar_utils::GetDayOfWeek(current_date) ==
      calendar_utils::kFirstDayOfWeekString)
    return;

  // Adds the first several days from the next month if the last day is not the
  // end day of this week. The end date of the last row should be 6 day's away
  // from the first day of this week. Adds 5 more hours just to cover the case
  // 25 hours in a day due to daylight saving.
  base::Time end_of_the_last_row_local =
      calendar_utils::GetFirstDayOfWeekLocalMidnight(current_date) +
      base::Days(6) + base::Hours(5) +
      base::Minutes(calendar_view_controller_->time_difference_minutes());
  base::Time::Exploded end_of_row_exploded =
      calendar_utils::GetExplodedUTC(end_of_the_last_row_local);

  // Gray-out dates in the last row, which are from the next month.
  while (current_date_exploded.day_of_month <=
         end_of_row_exploded.day_of_month) {
    // Next column is generated.
    AddDateCellToLayout(current_date, column,
                        /*is_in_current_month=*/false,
                        /*row_index=*/row_number);
    MoveToNextDay(column, current_date, current_date_local,
                  current_date_exploded);
  }
}

CalendarMonthView::~CalendarMonthView() {
  calendar_model_->CancelFetch(fetch_month_);
}

void CalendarMonthView::FetchEvents(const base::Time& month) {
  calendar_model_->FetchEvents({month});
}

void CalendarMonthView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time,
    const google_apis::calendar::EventList* events) {
  if (status == CalendarModel::kSuccess && start_time == fetch_month_)
    SchedulePaintChildren();
}

CalendarDateCellView* CalendarMonthView::AddDateCellToLayout(
    base::Time current_date,
    int column,
    bool is_in_current_month,
    int row_index) {
  auto* layout_manager = static_cast<views::TableLayout*>(GetLayoutManager());
  if (column == 0)
    layout_manager->AddRows(1, views::TableLayout::kFixedSize);
  return AddChildView(std::make_unique<CalendarDateCellView>(
      calendar_view_controller_, current_date,
      /*is_grayed_out_date=*/!is_in_current_month, /*row_index=*/row_index));
}

void CalendarMonthView::EnableFocus() {
  for (auto* cell : children())
    static_cast<CalendarDateCellView*>(cell)->EnableFocus();
}

void CalendarMonthView::DisableFocus() {
  for (auto* cell : children())
    static_cast<CalendarDateCellView*>(cell)->DisableFocus();
}

void CalendarMonthView::SchedulePaintChildren() {
  for (auto* cell : children())
    static_cast<CalendarDateCellView*>(cell)->MaybeSchedulePaint();
}

BEGIN_METADATA(CalendarDateCellView, views::View)
END_METADATA

}  // namespace ash
