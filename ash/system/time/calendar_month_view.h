// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

//  Container for `CalendarDateCellView` for a single month.
class ASH_EXPORT CalendarMonthView : public views::View {
 public:
  explicit CalendarMonthView(base::Time first_day_of_month);
  CalendarMonthView(const CalendarMonthView& other) = delete;
  CalendarMonthView& operator=(const CalendarMonthView& other) = delete;
  ~CalendarMonthView() override;

 private:
  // Adds the `current_date`'s `CalendarDateCellView` to the grid layout and
  // returns the next column set id.
  int AddDateCellToLayout(base::Time::Exploded current_date_exploded,
                          int column_set_id,
                          bool is_in_current_month);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
