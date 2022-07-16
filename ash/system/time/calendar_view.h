// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/callback_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace views {

class Label;

}  // namespace views

namespace ash {

class CalendarMonthView;

// This view displays a scrollable calendar.
class ASH_EXPORT CalendarView : public CalendarViewController::Observer,
                                public TrayDetailedView,
                                public views::ViewObserver {
 public:
  METADATA_HEADER(CalendarView);

  CalendarView(DetailedViewDelegate* delegate,
               UnifiedSystemTrayController* controller);
  CalendarView(const CalendarView& other) = delete;
  CalendarView& operator=(const CalendarView& other) = delete;
  ~CalendarView() override;

  void Init();

  // CalendarViewController::Observer:
  void OnMonthChanged(const base::Time::Exploded current_month) override;
  void OnEventsFetched(const google_apis::calendar::EventList* events) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewFocused(View* observed_view) override;

  // views::View:
  void OnThemeChanged() override;
  void OnEvent(ui::Event* event) override;

  // TrayDetailedView:
  void CreateExtraTitleRowButtons() override;
  views::Button* CreateInfoButton(views::Button::PressedCallback callback,
                                  int info_accessible_name_id) override;

  CalendarViewController* calendar_view_controller() {
    return calendar_view_controller_.get();
  }

 private:
  // The header of each month view which shows the month's name. If the year of
  // this month is not the same as the current month, the year is also shown in
  // this view.
  class MonthYearHeaderView;

  // The types to create the `MonthYearHeaderView` which are in corresponding to
  // the 3 months: `previous_month_`, `current_month_` and `next_month_`.
  enum LabelType { PREVIOUS, CURRENT, NEXT };

  friend class CalendarViewTest;

  // Assigns month views and labels based on the current date on screen.
  void SetMonthViews();

  // Returns the current month first row position.
  int PositionOfCurrentMonth();

  // Returns the today's row position.
  int PositionOfToday();

  // Adds a month label.
  views::View* AddLabelWithId(LabelType type, bool add_at_front = false);

  // Adds a `CalendarMonthView`.
  CalendarMonthView* AddMonth(base::Time month_first_date,
                              bool add_at_front = false);

  // Deletes the current next month and add a new month at the top of the
  // `content_view_`.
  void ScrollUpOneMonth();

  // Deletes the current previous month and adds a new month at the bottom of
  // the `content_view_`.
  void ScrollDownOneMonth();

  // Scrolls up one month then auto scroll to the current month's first row.
  void ScrollUpOneMonthAndAutoScroll();

  // Scrolls down one month then auto scroll to the current month's first row.
  void ScrollDownOneMonthAndAutoScroll();

  // Back to the landing view.
  void ResetToToday();

  // Auto scrolls to today. If the view is big enough we scroll to the first row
  // of today's month, otherwise we scroll to the position of today's row.
  void ScrollToToday();

  // If currently focusing on any date cell.
  bool IsDateCellViewFocused();

  // If focusing on `CalendarDateCellView` is interrupted (by scrolling or by
  // today's button), resets the content view's `FocusBehavior` to `ALWAYS`.
  void MaybeResetContentViewFocusBehavior();

  // We only fetch events after we've "settled" on the current on-screen month.
  void OnScrollingSettledTimerFired();

  // ScrollView callback.
  void OnContentsScrolled();

  // Unowned.
  UnifiedSystemTrayController* controller_;

  std::unique_ptr<CalendarViewController> calendar_view_controller_;

  // The content of the `scroll_view_`, which carries months and month labels.
  // Owned by `CalendarView`.
  views::View* content_view_ = nullptr;

  // The following is owned by `CalendarView`.
  views::ScrollView* scroll_view_ = nullptr;
  views::View* current_label_ = nullptr;
  views::View* previous_label_ = nullptr;
  views::View* next_label_ = nullptr;
  CalendarMonthView* previous_month_ = nullptr;
  CalendarMonthView* current_month_ = nullptr;
  CalendarMonthView* next_month_ = nullptr;
  views::Label* header_ = nullptr;
  views::Label* header_year_ = nullptr;
  views::Button* reset_to_today_button_ = nullptr;
  views::Button* settings_button_ = nullptr;
  TopShortcutButton* up_button_ = nullptr;
  TopShortcutButton* down_button_ = nullptr;

  // If it `is_resetting_scroll_`, we don't calculate the scroll position and we
  // don't need to check if we need to update the month or not.
  bool is_resetting_scroll_ = false;

  // Timer that fires when we've "settled" on, i.e. finished scrolling to, a
  // currently-visible month
  base::RetainingOneShotTimer scrolling_settled_timer_;
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::ScopedObservation<CalendarViewController,
                          CalendarViewController::Observer>
      scoped_calendar_view_controller_observer_{this};
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      scoped_view_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_VIEW_H_
