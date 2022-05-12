// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_view_controller.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// Renders a Calendar date cell. Pass in `true` as `is_grayed_out_date` if
// the date is not in the current month view's month.
class CalendarDateCellView : public CalendarViewController::Observer,
                             public views::LabelButton {
 public:
  METADATA_HEADER(CalendarDateCellView);

  CalendarDateCellView(CalendarViewController* calendar_view_controller,
                       base::Time date,
                       bool is_grayed_out_date,
                       int row_index);
  CalendarDateCellView(const CalendarDateCellView& other) = delete;
  CalendarDateCellView& operator=(const CalendarDateCellView& other) = delete;
  ~CalendarDateCellView() override;

  // views::View:
  void OnThemeChanged() override;

  // Draws the background for 'today'. If today is a grayed out date, which is
  // shown in its previous/next month, this background won't be drawn.
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // CalendarViewController::Observer:
  void OnSelectedDateUpdated() override;
  void CloseEventList() override;

  // Enables focus behavior of this cell.
  void EnableFocus();

  // Disables focus behavior of this cell.
  void DisableFocus();

  // Sets the tooltip label and a11y label based on the `event_number_`.
  void SetTooltipAndAccessibleName();

  // Schedule paint the cell to show the event dot.
  void MaybeSchedulePaint();

  // When focusing on the date cell for the first time, it shows "Use arrow keys
  // to navigate between dates" as instructions.
  void SetFirstOnFocusedAccessibilityLabel();

  // The row index in the date's month view.
  int row_index() const { return row_index_; }

 protected:
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // For unit tests.
  friend class CalendarMonthViewTest;

  // Callback called when this view is activated.
  void OnDateCellActivated(const ui::Event& event);

  // Computes the position of the indicator that the day has events.
  gfx::Point GetEventsPresentIndicatorCenterPosition();

  // Draw the indicator if the day has events.
  void MaybeDrawEventsIndicator(gfx::Canvas* canvas);

  // The date used to render this cell view.
  const base::Time date_;

  const bool grayed_out_;

  // The row index in the current month for this date cell. Starts from 0.
  const int row_index_;

  // If the current cell is selected.
  bool is_selected_ = false;

  // The number of event for `date_`.
  int event_number_ = 0;

  // The tool tip for this view. Before events data is back, only show date.
  // After the events date is back, show date and event numbers.
  std::u16string tool_tip_;

  // Owned by UnifiedCalendarViewController.
  CalendarViewController* const calendar_view_controller_;

  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};
};

//  Container for `CalendarDateCellView` for a single month.
class ASH_EXPORT CalendarMonthView : public views::View,
                                     public CalendarModel::Observer {
 public:
  CalendarMonthView(base::Time first_day_of_month,
                    CalendarViewController* calendar_view_controller);
  CalendarMonthView(const CalendarMonthView& other) = delete;
  CalendarMonthView& operator=(const CalendarMonthView& other) = delete;
  ~CalendarMonthView() override;

  // CalendarModel::Observer:
  void OnEventsFetched(const CalendarModel::FetchingStatus status,
                       const base::Time start_time,
                       const google_apis::calendar::EventList* events) override;

  // Enable each cell's focus behavior.
  void EnableFocus();

  // Disable each cell's focus behavior.
  void DisableFocus();

  // Re-paints the child cells.
  void SchedulePaintChildren();

  // Gets the cells of each row that should be first focused on.
  std::vector<CalendarDateCellView*> focused_cells() { return focused_cells_; }

  // If today's cell is in this view.
  bool has_today() { return has_today_; }

  // Returns the index of this month view's last row.
  int last_row_index() const { return last_row_index_; }

 private:
  // Adds the `current_date`'s `CalendarDateCellView` to the table layout and
  // returns it.
  CalendarDateCellView* AddDateCellToLayout(base::Time current_date,
                                            int column,
                                            bool is_in_current_month,
                                            int row_index);

  // Fetches events.
  void FetchEvents(const base::Time& month);

  // Owned by `CalendarView`.
  CalendarViewController* const calendar_view_controller_;

  // If today's cell is in this view.
  bool has_today_ = false;

  // The index of this month view's last row.
  int last_row_index_;

  // The cells of each row that should be first focused on. These
  // `CalendarDateCellView`s are the children of this view.
  std::vector<CalendarDateCellView*> focused_cells_;

  // UTC midnight to designate the month whose events will be fetched.
  base::Time fetch_month_;

  // Raw pointer to the (singleton) CalendarModel, to avoid a bunch of
  // daisy-chained calls to get the std::unique_ptr<>.
  CalendarModel* const calendar_model_;

  base::ScopedObservation<CalendarModel, CalendarModel::Observer>
      scoped_calendar_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_MONTH_VIEW_H_
