// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

namespace {

// The thickness of the today's circle line.
constexpr int kLineThickness = 2;

// The radius used to draw rounded today's circle
constexpr float kTodayRoundedRadius = 20.f;

// Radius of the small dot we display on a CalendarDateCellView if events are
// present for that day.
constexpr float kEventsPresentRoundedRadius = 2.f;

// The padding of the focus circle.
constexpr int kFocusCirclePadding = 4;

// Move to the next day. Both the column and the current date are moved to the
// next one.
void MoveToNextDay(int& column,
                   base::Time& current_date,
                   base::Time::Exploded& current_date_exploded) {
  current_date += base::Days(1);
  current_date.LocalExplode(&current_date_exploded);
  column = (column + 1) % calendar_utils::kDateInOneWeek;
}

}  // namespace

// TODO(https://crbug.com/1236276): Fix the ChromeVox window position on this
// view.
CalendarDateCellView::CalendarDateCellView(
    CalendarViewController* calendar_view_controller,
    base::Time::Exploded& date,
    bool is_grayed_out_date,
    int row_index)
    : views::LabelButton(
          views::Button::PressedCallback(
              is_grayed_out_date
                  ? base::DoNothing()
                  : base::BindRepeating(
                        &CalendarViewController::ShowEventListView,
                        base::Unretained(calendar_view_controller),
                        date,
                        row_index)),
          base::UTF8ToUTF16(base::NumberToString(date.day_of_month)),
          CONTEXT_CALENDAR_DATE),
      date_(date),
      grayed_out_(is_grayed_out_date),
      row_index_(row_index),
      calendar_view_controller_(calendar_view_controller) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(calendar_utils::kDateCellInsets));
  label()->SetElideBehavior(gfx::NO_ELIDE);
  label()->SetSubpixelRenderingEnabled(false);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColor(ColorProvider::Get()->GetControlsLayerColor(
      ColorProvider::ControlsLayerType::kFocusRingColor));
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::CircleHighlightPathGenerator>(
                gfx::Insets(kFocusCirclePadding)));

  DisableFocus();

  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
}

CalendarDateCellView::~CalendarDateCellView() = default;

void CalendarDateCellView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Gray-out the date that is not in the current month.
  SetEnabledTextColors(grayed_out_ ? calendar_utils::GetSecondaryTextColor()
                                   : calendar_utils::GetPrimaryTextColor());
}

// Draws the background for 'today'. If today is a grayed out date, which is
// shown in its previous/next month, we won't draw this background.
void CalendarDateCellView::OnPaintBackground(gfx::Canvas* canvas) {
  if (grayed_out_)
    return;

  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor bg_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  const SkColor border_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  const gfx::Rect content = GetContentsBounds();
  const gfx::Point center(
      (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
      (content.height() + calendar_utils::kDateVerticalPadding * 2) / 2);

  // If the view is focused or selected, paint a solid background.
  is_selected_ = calendar_utils::IsTheSameDay(
      date_, calendar_view_controller_->selected_date());

  // Sets accessible label. E.g. Calendar, week of July 16th 2021, [selected
  // date] is currently selected.
  if (is_selected_) {
    base::Time unexploded;
    bool result = base::Time::FromUTCExploded(date_, &unexploded);
    DCHECK(result);
    unexploded -= base::Days(date_.day_of_week);

    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_CALENDAR_SELECTED_DATE_CELL_ACCESSIBLE_DESCRIPTION,
        base::TimeFormatWithPattern(unexploded, "MMMMdyyyy"),
        base::UTF8ToUTF16(base::NumberToString(date_.day_of_month))));
  }

  if (views::View::HasFocus() || is_selected_) {
    // Change text color to the background color.
    const SkColor text_color = color_provider->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent90);
    SetEnabledTextColors(text_color);

    cc::PaintFlags background;
    background.setColor(border_color);
    background.setStyle(cc::PaintFlags::kFill_Style);
    background.setAntiAlias(true);
    canvas->DrawCircle(center, kTodayRoundedRadius, background);

    return;
  }

  SetEnabledTextColors(grayed_out_ ? calendar_utils::GetSecondaryTextColor()
                                   : calendar_utils::GetPrimaryTextColor());

  if (!calendar_utils::IsToday(date_))
    return;

  cc::PaintFlags highlight_background;
  highlight_background.setColor(bg_color);
  highlight_background.setStyle(cc::PaintFlags::kFill_Style);
  highlight_background.setAntiAlias(true);
  canvas->DrawCircle(center, kTodayRoundedRadius, highlight_background);

  cc::PaintFlags highlight_border;
  highlight_border.setColor(border_color);
  highlight_border.setAntiAlias(true);
  highlight_border.setStyle(cc::PaintFlags::kStroke_Style);
  highlight_border.setStrokeWidth(kLineThickness);
  canvas->DrawCircle(center, kTodayRoundedRadius, highlight_border);
}

void CalendarDateCellView::OnSelectedDateUpdated() {
  bool is_selected = calendar_utils::IsTheSameDay(
      date_, calendar_view_controller_->selected_date());
  // If the selected day changes, repaint the background.
  if (is_selected_ != is_selected) {
    is_selected_ = is_selected;
    SchedulePaint();
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

gfx::Point CalendarDateCellView::GetEventsPresentIndicatorCenterPosition() {
  const gfx::Rect content = GetContentsBounds();
  return gfx::Point(
      (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
      (content.height() + calendar_utils::kDateVerticalPadding * 2) / 2 +
          calendar_utils::kDateVerticalPadding);
}

void CalendarDateCellView::MaybeDrawEventsIndicator(gfx::Canvas* canvas) {
  if (grayed_out_)
    return;

  base::Time unexploded;
  bool result = base::Time::FromUTCExploded(date_, &unexploded);
  DCHECK(result);

  const int event_number =
      calendar_view_controller_->EventsNumberOfDay(unexploded,
                                                   /*events =*/nullptr);
  const int tooltip_id = (event_number <= 1)
                             ? IDS_ASH_CALENDAR_DATE_CELL_TOOLTIP
                             : IDS_ASH_CALENDAR_DATE_CELL_PLURAL_EVENTS_TOOLTIP;

  SetTooltipText(l10n_util::GetStringFUTF16(
      tooltip_id, base::TimeFormatWithPattern(unexploded, "MMMMdyyyy"),
      base::UTF8ToUTF16(base::NumberToString(event_number))));
  SetAccessibleName(l10n_util::GetStringFUTF16(
      tooltip_id, base::TimeFormatWithPattern(unexploded, "MMMMdyyyy"),
      base::UTF8ToUTF16(base::NumberToString(event_number))));

  if (event_number == 0)
    return;

  cc::PaintFlags indicator_paint_flags;
  indicator_paint_flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  indicator_paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(GetEventsPresentIndicatorCenterPosition(),
                     kEventsPresentRoundedRadius, indicator_paint_flags);
}

void CalendarDateCellView::PaintButtonContents(gfx::Canvas* canvas) {
  views::LabelButton::PaintButtonContents(canvas);
  MaybeDrawEventsIndicator(canvas);
}

CalendarMonthView::CalendarMonthView(
    const base::Time first_day_of_month,
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller) {
  views::TableLayout* layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  // The layer is required in animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  calendar_utils::SetUpWeekColumns(layout);

  base::Time::Exploded first_day_of_month_exploded =
      calendar_utils::GetExplodedLocal(first_day_of_month);

  // Calculates the start date.
  base::Time current_date =
      first_day_of_month - base::Days(first_day_of_month_exploded.day_of_week);
  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExplodedLocal(current_date);

  // TODO(https://crbug.com/1236276): Extract the following 3 parts (while
  // loops) into a method.
  int column = 0;
  // Gray-out dates in the first row, which are from the previous month.
  while (current_date_exploded.month % 12 ==
         (first_day_of_month_exploded.month - 1) % 12) {
    AddDateCellToLayout(current_date_exploded, column,
                        /*is_in_current_month=*/false, /*row_index=*/0);
    MoveToNextDay(column, current_date, current_date_exploded);
  }

  int row_number = 0;
  // Builds non-gray-out dates of the current month.
  while (current_date_exploded.month == first_day_of_month_exploded.month) {
    // Count a row when a new row starts.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      ++row_number;
    }
    auto* cell = AddDateCellToLayout(current_date_exploded, column,
                                     /*is_in_current_month=*/true,
                                     /*row_index=*/row_number - 1);
    // Add the first non-grayed-out cell of the row to the `focused_cells_`.
    if (column == 0 || current_date_exploded.day_of_month == 1) {
      focused_cells_.push_back(cell);
    }
    // If this row has today, updates today's row number and replaces today to
    // the last element in the `focused_cells_`.
    if (calendar_utils::IsToday(current_date_exploded)) {
      calendar_view_controller_->set_row_height(
          cell->GetPreferredSize().height());
      calendar_view_controller_->set_today_row(row_number);
      focused_cells_.back() = cell;
      has_today_ = true;
    }
    MoveToNextDay(column, current_date, current_date_exploded);
  }

  last_row_index_ = row_number - 1;

  // TODO(https://crbug.com/1236276): Handle some cases when the first day is
  // not Sunday.
  if (current_date_exploded.day_of_week == 0)
    return;

  // Adds the first several days from the next month if the last day is not the
  // end day of this week.
  const base::Time end_of_the_last_row =
      current_date + base::Days(6 - current_date_exploded.day_of_week);
  base::Time::Exploded end_of_row_exploded =
      calendar_utils::GetExplodedLocal(end_of_the_last_row);

  // Gray-out dates in the last row, which are from the next month.
  while (current_date_exploded.day_of_month <=
         end_of_row_exploded.day_of_month) {
    // Next column is generated.
    AddDateCellToLayout(current_date_exploded, column,
                        /*is_in_current_month=*/false,
                        /*row_index=*/row_number);
    MoveToNextDay(column, current_date, current_date_exploded);
  }
}

CalendarMonthView::~CalendarMonthView() = default;

CalendarDateCellView* CalendarMonthView::AddDateCellToLayout(
    base::Time::Exploded current_date_exploded,
    int column,
    bool is_in_current_month,
    int row_index) {
  auto* layout_manager = static_cast<views::TableLayout*>(GetLayoutManager());
  if (column == 0)
    layout_manager->AddRows(1, views::TableLayout::kFixedSize);
  return AddChildView(std::make_unique<CalendarDateCellView>(
      calendar_view_controller_, current_date_exploded,
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

BEGIN_METADATA(CalendarDateCellView, views::View)
END_METADATA

}  // namespace ash
