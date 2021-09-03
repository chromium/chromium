// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace views {

class Label;

}  // namespace views

namespace ash {

class CalendarMonthView;

// This view displays a scrollable calendar.
class ASH_EXPORT CalendarView : public TrayDetailedView,
                                views::ScrollView::Observer,
                                CalendarViewController::Observer {
 public:
  METADATA_HEADER(CalendarView);

  CalendarView(DetailedViewDelegate* delegate,
               CalendarViewController* calendar_view_controller);
  CalendarView(const CalendarView& other) = delete;
  CalendarView& operator=(const CalendarView& other) = delete;
  ~CalendarView() override;

  // views::ScrollView::Observer:
  void OnContentsScrolled() override;

  // CalendarViewController::Observer:
  void OnMonthChanged(const base::Time::Exploded current_month) override;

  // views::View:
  void OnThemeChanged() override;

  // Inits the views and auto scroll to the current date.
  void Init();

 private:
  friend class CalendarViewTest;

  // Assigns month views and labels based on the current date on screen.
  void SetMonthViews();

  // Returns the today's month position.
  int PositionOfToday();

  // Adds a month label.
  views::Label* AddLabelWithId(std::u16string label_string,
                               bool add_at_front = false);

  // Adds a `CalendarMonthView`.
  CalendarMonthView* AddMonth(base::Time month_first_date,
                              bool add_at_front = false);

  // Deletes the current next month and add a new month at the top of the
  // `content_view_`.
  void ScrollUpOneMonth();

  // Deletes the current previous month and adds a new month at the bottom of
  // the `content_view_`.
  void ScrollDownOneMonth();

  // Owned by `UnifiedCalendarViewController`.
  CalendarViewController* const calendar_view_controller_;

  // Owned by `CalendarView`.
  views::ScrollView* scroll_view_ = nullptr;

  // The content of the `scroll_view_`, which carries months and month labels.
  // Owned by `CalendarView`.
  views::View* content_view_ = nullptr;

  // The followings are owned by `CalendarView`.
  views::Label* current_label_ = nullptr;
  views::Label* previous_label_ = nullptr;
  views::Label* next_label_ = nullptr;
  CalendarMonthView* previous_month_ = nullptr;
  CalendarMonthView* current_month_ = nullptr;
  CalendarMonthView* next_month_ = nullptr;

  // If it `is_resetting_scroll_`, we don't calculate the scroll position and we
  // don't need to check if we need to update the month or not.
  bool is_resetting_scroll_ = false;

  base::ScopedObservation<views::ScrollView,
                          views::ScrollView::Observer,
                          &views::ScrollView::AddScrollViewObserver,
                          &views::ScrollView::RemoveScrollViewObserver>
      scoped_scroll_view_observer_{this};
  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
