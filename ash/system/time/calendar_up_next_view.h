// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_
#define ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ui/views/view.h"

namespace ash {

// This view displays a scrollable list of `CalendarEventListItemView` for the
// events that a user has coming up, either imminently or that are already in
// progress but not yet finished.
class ASH_EXPORT CalendarUpNextView : public views::View {
 public:
  METADATA_HEADER(CalendarUpNextView);

  explicit CalendarUpNextView(CalendarViewController* calendar_view_controller);
  CalendarUpNextView(const CalendarUpNextView& other) = delete;
  CalendarUpNextView& operator=(const CalendarUpNextView& other) = delete;
  ~CalendarUpNextView() override;

  // views::View
  void OnThemeChanged() override;
  void Layout() override;

 private:
  friend class CalendarUpNextViewTest;

  void UpdateEvents();

  // Owned by `CalendarView`.
  CalendarViewController* calendar_view_controller_;

  // Owned by `CalendarUpNextView`.
  views::View* const header_view_;
  views::ScrollView* const scroll_view_;
  // The content of the horizontal `scroll_view`, which carries a list of
  // `CalendarEventListItemView`.
  views::View* const content_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_H_
