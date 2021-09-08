// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_month_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/time/calendar_utils.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// Number of days in one week.
constexpr int kDateInOneWeek = 7;

// The thickness of the today's circle line.
constexpr int kLineThickness = 2;

// The radius used to draw rounded today's circle
constexpr float kTodayRoundedRadius = 20.f;

// The padding in each date cell view.
constexpr int kDateVerticalPadding = 13;
constexpr int kDateHorizontalPadding = 2;
}  // namespace

using views::GridLayout;

// Renders a Calendar date cell. Pass in `true` as `is_grayed_out_date` if
// the date is not in the current month view's month.
// TODO(https://crbug.com/1236276): enable focusing, add has_event dot,
// on-select effect.
class CalendarDateCellView : public views::LabelButton {
 public:
  CalendarDateCellView(base::Time::Exploded& date, bool is_grayed_out_date)
      : views::LabelButton(
            views::Button::PressedCallback(base::BindRepeating([]() {
              // TODO(https://crbug.com/1238927): Add a menthod in the
              // controller to open the expandable view and call it here.
            })),
            base::UTF8ToUTF16(base::NumberToString(date.day_of_month))),
        date_(date),
        grayed_out_(is_grayed_out_date) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetBorder(
        views::CreateEmptyBorder(kDateVerticalPadding, kDateHorizontalPadding,
                                 kDateVerticalPadding, kDateHorizontalPadding));
    label()->SetElideBehavior(gfx::NO_ELIDE);
    label()->SetSubpixelRenderingEnabled(false);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  }

  ~CalendarDateCellView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const AshColorProvider* color_provider = AshColorProvider::Get();
    const SkColor primary_text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    const SkColor secondary_text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary);

    // Gray-out the date that is not in the current month.
    SetEnabledTextColors(grayed_out_ ? primary_text_color
                                     : secondary_text_color);
  }

  // Draws the background for 'today'. If today is a grayed out date, which is
  // shown in its previous/next month, we won't draw this background.
  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (!calendar_utils::IsToday(date_) || grayed_out_)
      return;

    const AshColorProvider* color_provider = AshColorProvider::Get();
    const SkColor bg_color = color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
    const SkColor border_color = color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor);

    cc::PaintFlags highlight_background;
    const gfx::Rect content = GetContentsBounds();
    gfx::Point center((content.width() + kDateHorizontalPadding * 2) / 2,
                      (content.height() + kDateVerticalPadding * 2) / 2);

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

 private:
  // The date used to render this cell view.
  base::Time::Exploded date_;

  const bool grayed_out_;
};

CalendarMonthView::CalendarMonthView(const base::Time first_day_of_month) {
  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(0);

  // Set up the `GridLayout` to have 7 columns, which is one week row (7 days).
  for (int i = 0; i < kDateInOneWeek; i++) {
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::ColumnSize::kFixed, 0, 0);
    column_set->AddPaddingColumn(0, 2);
  }

  base::Time::Exploded first_day_of_month_exploded =
      calendar_utils::GetExploded(first_day_of_month);

  // Calculates the start date.
  const base::Time start_of_the_first_row =
      first_day_of_month -
      base::TimeDelta::FromDays(first_day_of_month_exploded.day_of_week);
  base::Time::Exploded start_of_row_exploded =
      calendar_utils::GetExploded(start_of_the_first_row);

  base::Time current_date = start_of_the_first_row;
  base::Time::Exploded current_date_exploded =
      calendar_utils::GetExploded(current_date);

  int column_set_id = 0;
  // Gray-out dates in the first row, which are from the previous month, and the
  // non-gray-out dates of the current month.
  while (current_date_exploded.month == start_of_row_exploded.month ||
         current_date_exploded.month == first_day_of_month_exploded.month) {
    // Next column set id is generated.
    column_set_id = AddDateCellToLayout(
        current_date_exploded, column_set_id,
        /*is_in_current_month=*/current_date_exploded.month ==
            first_day_of_month_exploded.month);
    current_date += base::TimeDelta::FromDays(1);
    current_date.LocalExplode(&current_date_exploded);
  }

  // TODO(https://crbug.com/1236276): Handle some cases when the first day is
  // not Sunday.
  if (current_date_exploded.day_of_week == 0)
    return;

  // Adds the first several days from the next month if the last day is not the
  // end day of this week.
  const base::Time end_of_the_last_row =
      current_date +
      base::TimeDelta::FromDays(6 - current_date_exploded.day_of_week);
  base::Time::Exploded end_of_row_exploded =
      calendar_utils::GetExploded(end_of_the_last_row);

  // Gray-out dates in the last row, which are from the next month.
  while (current_date_exploded.day_of_month <=
         end_of_row_exploded.day_of_month) {
    // Next column set id is generated.
    column_set_id = AddDateCellToLayout(current_date_exploded, column_set_id,
                                        /*is_in_current_month=*/false);
    current_date += base::TimeDelta::FromDays(1);
    current_date.LocalExplode(&current_date_exploded);
  }
}

CalendarMonthView::~CalendarMonthView() = default;

int CalendarMonthView::AddDateCellToLayout(
    base::Time::Exploded current_date_exploded,
    int column_set_id,
    bool is_in_current_month) {
  GridLayout* layout_manager = static_cast<GridLayout*>(GetLayoutManager());
  if (column_set_id == 0)
    layout_manager->StartRow(0, 0);

  layout_manager->AddView(std::make_unique<CalendarDateCellView>(
      current_date_exploded,
      /*is_grayed_out_date=*/is_in_current_month));

  return (column_set_id + 1) % kDateInOneWeek;
}

}  // namespace ash
