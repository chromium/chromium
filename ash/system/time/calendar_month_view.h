// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace ash {

class CalendarViewController;

// Renders a Calendar date cell. Pass in `true` as `is_grayed_out_date` if
// the date is not in the current month view's month.
class CalendarDateCellView : public views::LabelButton {
 public:
  METADATA_HEADER(CalendarDateCellView);

  CalendarDateCellView(base::Time::Exploded& date, bool is_grayed_out_date);
  CalendarDateCellView(const CalendarDateCellView& other) = delete;
  CalendarDateCellView& operator=(const CalendarDateCellView& other) = delete;
  ~CalendarDateCellView() override;

  // views::View:
  void OnThemeChanged() override;

  // Draws the background for 'today'. If today is a grayed out date, which is
  // shown in its previous/next month, we won't draw this background.
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // Enables focus behavior of this cell.
  void EnableFocus();

  // Disables focus behavior of this cell.
  void DisableFocus();

 private:
  // The date used to render this cell view.
  const base::Time::Exploded date_;

  const bool grayed_out_;
};

//  Container for `CalendarDateCellView` for a single month.
class ASH_EXPORT CalendarMonthView : public views::View {
 public:
  CalendarMonthView(base::Time first_day_of_month,
                    CalendarViewController* calendar_view_controller);
  CalendarMonthView(const CalendarMonthView& other) = delete;
  CalendarMonthView& operator=(const CalendarMonthView& other) = delete;
  ~CalendarMonthView() override;

  // Enable each cell's focus behavior.
  void EnableFocus();

  // Disable each cell's focus behavior.
  void DisableFocus();

  // Gets the cells of each row that should be first focused on.
  std::vector<CalendarDateCellView*> focused_cells() { return focused_cells_; }

  // If today's cell is in this view.
  bool has_today() { return has_today_; }

 private:
  // Adds the `current_date`'s `CalendarDateCellView` to the grid layout and
  // returns the next column set id.
  CalendarDateCellView* AddDateCellToLayout(
      base::Time::Exploded current_date_exploded,
      int column_set_id,
      bool is_in_current_month);

  // Owned by `UnifiedCalendarViewController`.
  CalendarViewController* const calendar_view_controller_;

  // If today's cell is in this view.
  bool has_today_ = false;

  // The cells of each row that should be first focused on. These
  // `CalendarDateCellView`s are the children of this view.
  std::vector<CalendarDateCellView*> focused_cells_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
