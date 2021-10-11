// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// The thickness of the today's circle line.
constexpr int kLineThickness = 2;

// The radius used to draw rounded today's circle
constexpr float kTodayRoundedRadius = 20.f;

// The padding of the focus circle.
constexpr int kFocusCirclePadding = 4;

// Move to the next day. Both the column set and the current date are
// moved to the next one.
void MoveToNextDay(int& column_set_id,
                   base::Time& current_date,
                   base::Time::Exploded& current_date_exploded) {
  current_date += base::Days(1);
  current_date.LocalExplode(&current_date_exploded);
  column_set_id = (column_set_id + 1) % calendar_utils::kDateInOneWeek;
}

}  // namespace

using views::GridLayout;

// TODO(https://crbug.com/1236276): Fix the ChromeVox window position on this
// view.
CalendarDateCellView::CalendarDateCellView(base::Time::Exploded& date,
                                           bool is_grayed_out_date)
    : views::LabelButton(
          views::Button::PressedCallback(base::BindRepeating([]() {
            // TODO(https://crbug.com/1238927): Add a menthod in the
            // controller to open the expandable view and call it here.
          })),
          base::UTF8ToUTF16(base::NumberToString(date.day_of_month)),
          CONTEXT_CALENDAR_DATE),
      date_(date),
      grayed_out_(is_grayed_out_date) {
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
  const AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor bg_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  const SkColor border_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  // If the view is focused, paint a solid background.
  if (views::View::HasFocus()) {
    // Change text color to the background color.
    const SkColor text_color = color_provider->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent90);
    SetEnabledTextColors(text_color);

    cc::PaintFlags background;
    const gfx::Rect content = GetContentsBounds();
    const gfx::Point center(
        (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
        (content.height() + calendar_utils::kDateVerticalPadding * 2) / 2);

    background.setColor(border_color);
    background.setStyle(cc::PaintFlags::kFill_Style);
    background.setAntiAlias(true);
    canvas->DrawCircle(center, kTodayRoundedRadius, background);

    return;
  }

  SetEnabledTextColors(grayed_out_ ? calendar_utils::GetSecondaryTextColor()
                                   : calendar_utils::GetPrimaryTextColor());

  if (grayed_out_ || !calendar_utils::IsToday(date_))
    return;

  cc::PaintFlags highlight_background;
  const gfx::Rect content = GetContentsBounds();
  gfx::Point center(
      (content.width() + calendar_utils::kDateHorizontalPadding * 2) / 2,
      (content.height() + calendar_utils::kDateVerticalPadding * 2) / 2);

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

void CalendarDateCellView::EnableFocus() {
  if (grayed_out_)
    return;
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarDateCellView::DisableFocus() {
  SetFocusBehavior(FocusBehavior::NEVER);
}

CalendarMonthView::CalendarMonthView(
    const base::Time first_day_of_month,
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller) {
  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(0);

  calendar_utils::SetUpWeekColumnSets(column_set);

  base::Time::Exploded first_day_of_month_exploded =
      calendar_utils::GetExploded(first_day_of_month);

  // Calculates the start date.
  base::Time current_date =
      first_day_of_month - base::Days(first_day_of_month_exploded.day_of_week);
  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExploded(current_date);

  // TODO(https://crbug.com/1236276): Extract the following 3 parts (while
  // loops) into a method.
  int column_set_id = 0;
  // Gray-out dates in the first row, which are from the previous month.
  while (current_date_exploded.month % 12 ==
         (first_day_of_month_exploded.month - 1) % 12) {
    AddDateCellToLayout(current_date_exploded, column_set_id,
                        /*is_in_current_month=*/false);
    MoveToNextDay(column_set_id, current_date, current_date_exploded);
  }

  int row_number = 0;
  // Builds non-gray-out dates of the current month.
  while (current_date_exploded.month == first_day_of_month_exploded.month) {
    auto* cell = AddDateCellToLayout(current_date_exploded, column_set_id,
                                     /*is_in_current_month=*/true);
    // Add the first non-grayed-out cell of the row to the `focused_cells_`.
    if (column_set_id == 0 || current_date_exploded.day_of_month == 1) {
      focused_cells_.push_back(cell);
      // Count a row when a new row starts.
      ++row_number;
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
    MoveToNextDay(column_set_id, current_date, current_date_exploded);
  }

  // TODO(https://crbug.com/1236276): Handle some cases when the first day is
  // not Sunday.
  if (current_date_exploded.day_of_week == 0)
    return;

  // Adds the first several days from the next month if the last day is not the
  // end day of this week.
  const base::Time end_of_the_last_row =
      current_date + base::Days(6 - current_date_exploded.day_of_week);
  base::Time::Exploded end_of_row_exploded =
      calendar_utils::GetExploded(end_of_the_last_row);

  // Gray-out dates in the last row, which are from the next month.
  while (current_date_exploded.day_of_month <=
         end_of_row_exploded.day_of_month) {
    // Next column set id is generated.
    AddDateCellToLayout(current_date_exploded, column_set_id,
                        /*is_in_current_month=*/false);
    MoveToNextDay(column_set_id, current_date, current_date_exploded);
  }
}

CalendarMonthView::~CalendarMonthView() = default;

CalendarDateCellView* CalendarMonthView::AddDateCellToLayout(
    base::Time::Exploded current_date_exploded,
    int column_set_id,
    bool is_in_current_month) {
  GridLayout* layout_manager = static_cast<GridLayout*>(GetLayoutManager());
  if (column_set_id == 0)
    layout_manager->StartRow(0, 0);
  return layout_manager->AddView(std::make_unique<CalendarDateCellView>(
      current_date_exploded,
      /*is_grayed_out_date=*/!is_in_current_month));
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
