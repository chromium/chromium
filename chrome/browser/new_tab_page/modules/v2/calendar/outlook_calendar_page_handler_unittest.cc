// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"

class OutlookCalendarPageHandlerTest : public testing::Test {
 public:
  OutlookCalendarPageHandlerTest() = default;

  std::unique_ptr<OutlookCalendarPageHandler> CreateHandler() {
    return std::make_unique<OutlookCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::OutlookCalendarPageHandler>());
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OutlookCalendarPageHandlerTest, GetFakeEvents) {
  // kNtpOutlookCalendarModuleDataParam must have a value to use fake events.
  feature_list().InitAndEnableFeatureWithParameters(
      ntp_features::kNtpOutlookCalendarModule,
      {{ntp_features::kNtpOutlookCalendarModuleDataParam, "fake"}});
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  handler->GetEvents(future.GetCallback());
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(events[i]->title, "Calendar Event " + base::NumberToString(i));
    EXPECT_EQ(events[i]->start_time, base::Time::Now() + base::Minutes(i * 30));
    EXPECT_EQ(events[i]->end_time, events[i]->start_time + base::Minutes(30));
    EXPECT_EQ(events[i]->url,
              GURL("https://foo.com/" + base::NumberToString(i)));
    EXPECT_EQ(events[i]->attachments.size(), 3u);
    for (int j = 0; j < 3; ++j) {
      ntp::calendar::mojom::AttachmentPtr attachment =
          std::move(events[i]->attachments[j]);
      EXPECT_EQ(attachment->title, "Attachment " + base::NumberToString(j));
      EXPECT_EQ(attachment->resource_url,
                "https://foo.com/attachment" + base::NumberToString(j));
    }
    EXPECT_EQ(events[i]->conference_url,
              GURL("https://foo.com/conference" + base::NumberToString(i)));
    EXPECT_TRUE(events[i]->is_accepted);
    EXPECT_FALSE(events[i]->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, GetEvents) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  handler->GetEvents(future.GetCallback());
  EXPECT_EQ(future.Get().size(), 0u);
}
