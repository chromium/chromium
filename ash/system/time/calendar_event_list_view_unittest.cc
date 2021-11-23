// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList() {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("America/Los_Angeles");

  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Nov 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "18 Nov 2021 10:30 GMT", "18 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_2", "summary_2", "18 Nov 2021 11:30 GMT", "18 Nov 2021 12:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_3", "summary_3", "19 Nov 2021 8:30 GMT", "19 Nov 2021 10:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_4", "summary_4", "21 Nov 2021 8:30 GMT", "21 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_5", "summary_5", "21 Nov 2021 10:30 GMT", "21 Nov 2021 11:30 GMT"));

  return event_list;
}

}  // namespace

class CalendarViewEventListViewTest : public AshTestBase {
 public:
  CalendarViewEventListViewTest() = default;
  CalendarViewEventListViewTest(const CalendarViewEventListViewTest&) = delete;
  CalendarViewEventListViewTest& operator=(
      const CalendarViewEventListViewTest&) = delete;
  ~CalendarViewEventListViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    event_list_view_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateEventListView(base::Time date) {
    event_list_view_.reset();
    controller_->UpdateMonth(date);
    controller_->InsertEvents(CreateMockEventList());
    event_list_view_ =
        std::make_unique<CalendarEventListView>(controller_.get());
  }

  void SetSelectedDate(base::Time::Exploded date) {
    controller_->selected_date_ = date;
    controller_->ShowEventListView(date, /*row_index=*/0);
  }

  CalendarEventListView* event_list_view() { return event_list_view_.get(); }
  views::View* content_view() { return event_list_view_->content_view_; }
  CalendarViewController* controller() { return controller_.get(); }

  views::Label* GetSummary(int child_idex) {
    return static_cast<views::Label*>(
        static_cast<CalendarEventListItemView*>(
            content_view()->children()[child_idex])
            ->summary_);
  }

 private:
  std::unique_ptr<CalendarEventListView> event_list_view_;
  std::unique_ptr<CalendarViewController> controller_;
};

TEST_F(CalendarViewEventListViewTest, ShowEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));

  CreateEventListView(date);

  EXPECT_EQ(0u, content_view()->children().size());

  base::Time::Exploded selected_date;
  date.LocalExplode(&selected_date);
  SetSelectedDate(selected_date);

  // 3 events on 18 Nov 2021.
  EXPECT_EQ(3u, content_view()->children().size());
  EXPECT_EQ(u"summary_0", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_1", GetSummary(1)->GetText());
  EXPECT_EQ(u"summary_2", GetSummary(2)->GetText());

  (date + base::Days(1)).LocalExplode(&selected_date);
  SetSelectedDate(selected_date);

  // 1 event on 19 Nov 2021.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(u"summary_3", GetSummary(0)->GetText());

  (date + base::Days(2)).LocalExplode(&selected_date);
  SetSelectedDate(selected_date);

  // 0 event on 20 Nov 2021.
  EXPECT_EQ(0u, content_view()->children().size());

  (date + base::Days(3)).LocalExplode(&selected_date);
  SetSelectedDate(selected_date);

  // 2 events on 21 Nov 2021.
  EXPECT_EQ(2u, content_view()->children().size());
  EXPECT_EQ(u"summary_4", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_5", GetSummary(1)->GetText());
}

}  // namespace ash
