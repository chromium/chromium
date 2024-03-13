// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_ranker.h"

#include <limits>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  CHECK(base::Time::FromString(time_string, &time));
  return time;
}

TEST(BirchRankerTest, IsMorning) {
  // Simulate 8 AM local time.
  base::Time morning = base::Time::Now().LocalMidnight() + base::Hours(8);
  BirchRanker morning_ranker(morning);
  EXPECT_TRUE(morning_ranker.IsMorning());

  // Simulate 1 PM local time.
  base::Time afternoon = base::Time::Now().LocalMidnight() + base::Hours(13);
  BirchRanker afternoon_ranker(afternoon);
  EXPECT_FALSE(afternoon_ranker.IsMorning());
}

TEST(BirchRankerTest, IsEvening) {
  // Simulate 8 AM local time.
  base::Time morning = base::Time::Now().LocalMidnight() + base::Hours(8);
  BirchRanker morning_ranker(morning);
  EXPECT_FALSE(morning_ranker.IsEvening());

  // Simulate 6 PM local time.
  base::Time evening = base::Time::Now().LocalMidnight() + base::Hours(18);
  BirchRanker evening_ranker(evening);
  EXPECT_TRUE(evening_ranker.IsEvening());
}

TEST(BirchRankerTest, RankCalendarItems_Morning) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 9 AM in the morning.
  base::Time now = TimeFromString("22 Feb 2024 9:00 UTC");
  BirchRanker ranker(now);
  ASSERT_TRUE(ranker.IsMorning());

  // Create an ongoing event (8:00 - 11:00).
  BirchCalendarItem item0(
      u"Ongoing",
      /*start_time=*/TimeFromString("22 Feb 2024 08:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 11:00 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Create an upcoming event (10:00 - 10:30).
  BirchCalendarItem item1(
      u"Upcoming",
      /*start_time=*/TimeFromString("22 Feb 2024 10:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 10:30 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Create another event later in the day. It isn't the first one, so it won't
  // be ranked.
  BirchCalendarItem item2(
      u"Generic",
      /*start_time=*/TimeFromString("22 Feb 2024 13:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 13:30 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchCalendarItem> items = {item2, item1, item0};

  ranker.RankCalendarItems(&items);

  ASSERT_EQ(3u, items.size());

  // The ongoing and upcoming events have the same ranking.
  EXPECT_FLOAT_EQ(items[0].ranking(), 6.f);
  EXPECT_FLOAT_EQ(items[1].ranking(), 6.f);

  // The generic event wasn't ranked, so has the default value for ranking.
  EXPECT_FLOAT_EQ(items[2].ranking(), std::numeric_limits<float>::max());
}

TEST(BirchRankerTest, RankCalendarItems_Evening) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 6 PM in the evening.
  base::Time now = TimeFromString("22 Feb 2024 18:00 UTC");
  BirchRanker ranker(now);
  ASSERT_TRUE(ranker.IsEvening());

  // Create an event starting in the next 30 minutes (6:15 PM).
  BirchCalendarItem item0(
      u"Soon",
      /*start_time=*/TimeFromString("22 Feb 2024 18:15 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 18:45 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Create an event starting more than 30 minutes from now (7 PM).
  BirchCalendarItem item1(
      u"Later",
      /*start_time=*/TimeFromString("22 Feb 2024 19:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 19:30 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Create an event for 9 AM tomorrow morning.
  BirchCalendarItem item2(
      u"Tomorrow",
      /*start_time=*/TimeFromString("23 Feb 2024 09:00 UTC"),
      /*end_time=*/TimeFromString("23 Feb 2024 09:30 UTC"),
      /*calendar_url=*/GURL(),
      /*conference_url=*/GURL());

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchCalendarItem> items = {item2, item1, item0};

  ranker.RankCalendarItems(&items);

  ASSERT_EQ(3u, items.size());

  // The soon event has a ranking.
  EXPECT_FLOAT_EQ(items[0].ranking(), 12.f);

  // The later event has no ranking, it was too far in the future.
  EXPECT_FLOAT_EQ(items[1].ranking(), std::numeric_limits<float>::max());

  // The tomorrow event has a ranking.
  EXPECT_FLOAT_EQ(items[2].ranking(), 25.f);
}

TEST(BirchRankerTest, RankCalendarItems_OngoingInAfternoon) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 3 PM.
  base::Time now = TimeFromString("22 Feb 2024 15:00 UTC");
  BirchRanker ranker(now);

  // Create an ongoing event (2 PM to 4 PM).
  BirchCalendarItem item(u"Ongoing",
                         /*start_time=*/TimeFromString("22 Feb 2024 14:00 UTC"),
                         /*end_time=*/TimeFromString("22 Feb 2024 16:00 UTC"),
                         /*calendar_url=*/GURL(),
                         /*conference_url=*/GURL());
  std::vector<BirchCalendarItem> items = {item};

  ranker.RankCalendarItems(&items);

  ASSERT_EQ(1u, items.size());

  // The ongoing event has a ranking.
  EXPECT_FLOAT_EQ(items[0].ranking(), 9.f);
}

TEST(BirchRankerTest, RankAttachmentItems_Morning) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 9 AM in the morning.
  base::Time now = TimeFromString("22 Feb 2024 9:00 UTC");
  BirchRanker ranker(now);
  ASSERT_TRUE(ranker.IsMorning());

  // Create an attachment for an ongoing event (8 AM to 10 AM).
  BirchAttachmentItem item0(
      u"Ongoing",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 08:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 10:00 UTC"));

  // Create an attachment for an upcoming event (9:15 to 9:45).
  BirchAttachmentItem item1(
      u"Upcoming",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 09:15 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 09:45 UTC"));

  // Create an attachment for another event later in the day (1 PM).
  BirchAttachmentItem item2(
      u"Later",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 13:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 13:30 UTC"));

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchAttachmentItem> items = {item2, item1, item0};

  ranker.RankAttachmentItems(&items);

  ASSERT_EQ(3u, items.size());

  // The ongoing event's item has a high priority.
  EXPECT_FLOAT_EQ(items[0].ranking(), 7.f);

  // The upcoming event's item has a medium priority.
  EXPECT_FLOAT_EQ(items[1].ranking(), 13.f);

  // The later event's item wasn't ranked, so has the default value.
  EXPECT_FLOAT_EQ(items[2].ranking(), std::numeric_limits<float>::max());
}

TEST(BirchRankerTest, RankAttachmentItems_Evening) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 6 PM in the evening.
  base::Time now = TimeFromString("22 Feb 2024 18:00 UTC");
  BirchRanker ranker(now);
  ASSERT_TRUE(ranker.IsEvening());

  // Create an attachment for an ongoing event (5 PM to 7 PM).
  BirchAttachmentItem item0(
      u"Ongoing",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 17:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 19:00 UTC"));

  // Create an attachment for an upcoming event (6:15 PM).
  BirchAttachmentItem item1(
      u"Upcoming",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 18:15 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 18:45 UTC"));

  // Create an attachment for another event later in the evening (8 PM).
  BirchAttachmentItem item2(
      u"Later",
      /*file_url=*/GURL(),
      /*icon_url=*/GURL(),
      /*start_time=*/TimeFromString("22 Feb 2024 20:00 UTC"),
      /*end_time=*/TimeFromString("22 Feb 2024 20:30 UTC"));

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchAttachmentItem> items = {item2, item1, item0};

  ranker.RankAttachmentItems(&items);

  ASSERT_EQ(3u, items.size());

  // The ongoing event's item has a medium priority.
  EXPECT_FLOAT_EQ(items[0].ranking(), 10.f);

  // The upcoming event's item has a lower priority.
  EXPECT_FLOAT_EQ(items[1].ranking(), 13.f);

  // The later event's item wasn't ranked, so has the default value.
  EXPECT_FLOAT_EQ(items[2].ranking(), std::numeric_limits<float>::max());
}

TEST(BirchRankerTest, RankFileSuggestItems) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 9 AM.
  base::Time now = TimeFromString("22 Feb 2024 09:00 UTC");
  BirchRanker ranker(now);

  // Create a file shared in the last hour.
  BirchFileItem item0(base::FilePath("/item0"), u"suggested",
                      TimeFromString("22 Feb 2024 08:45 UTC"));

  // Create a file shared in the last day.
  BirchFileItem item1(base::FilePath("/item1"), u"suggested",
                      TimeFromString("21 Feb 2024 09:15 UTC"));

  // Create a file shared in the last week.
  BirchFileItem item2(base::FilePath("/item2"), u"suggested",
                      TimeFromString("15 Feb 2024 09:15 UTC"));

  // Create a file shared more than a week ago.
  BirchFileItem item3(base::FilePath("/item3"), u"suggested",
                      TimeFromString("14 Feb 2024 09:15 UTC"));

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchFileItem> items = {item3, item2, item1, item0};

  ranker.RankFileSuggestItems(&items);

  ASSERT_EQ(4u, items.size());

  // The file shared in the last hour has high priority.
  EXPECT_EQ(items[0].title(), u"item0");
  EXPECT_FLOAT_EQ(items[0].ranking(), 19.f);

  // The file shared in the last day has medium priority.
  EXPECT_EQ(items[1].title(), u"item1");
  EXPECT_FLOAT_EQ(items[1].ranking(), 32.f);

  // The file shared in the last week has low priority.
  EXPECT_EQ(items[2].title(), u"item2");
  EXPECT_FLOAT_EQ(items[2].ranking(), 40.f);

  // The file shared more than a week ago wasn't ranked.
  EXPECT_EQ(items[3].title(), u"item3");
  EXPECT_FLOAT_EQ(items[3].ranking(), std::numeric_limits<float>::max());
}

TEST(BirchRankerTest, RankRecentTabItems) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 9 AM.
  base::Time now = TimeFromString("22 Feb 2024 09:00 UTC");
  BirchRanker ranker(now);

  // Create tab with a timestamp in the last 5 minutes.
  BirchTabItem item0(u"item0", GURL(), TimeFromString("22 Feb 2024 08:59 UTC"),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);

  // Create a tab with timestamp in the last hour.
  BirchTabItem item1(u"item1", GURL(), TimeFromString("22 Feb 2024 08:30 UTC"),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);

  // Create a tab with timestamp in the last day.
  BirchTabItem item2(u"item2", GURL(), TimeFromString("21 Feb 2024 09:01 UTC"),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);

  // Create a tab with timestamp more than a day ago.
  BirchTabItem item3(u"item3", GURL(), TimeFromString("21 Feb 2024 08:59 UTC"),
                     GURL(), "", BirchTabItem::DeviceFormFactor::kDesktop);

  // Put the items in the vector in reverse order to validate that they are
  // still handled in the correct order (by time) inside the ranker.
  std::vector<BirchTabItem> items = {item3, item2, item1, item0};

  ranker.RankRecentTabItems(&items);

  ASSERT_EQ(4u, items.size());

  // The tab with a timestamp in the last 5 minutes has high priority.
  EXPECT_EQ(items[0].title(), u"item0");
  EXPECT_FLOAT_EQ(items[0].ranking(), 14.f);

  // The tab with a timestamp in the last hour has medium priority.
  EXPECT_EQ(items[1].title(), u"item1");
  EXPECT_FLOAT_EQ(items[1].ranking(), 17.f);

  // The tab with a timestamp in the last day has low priority.
  EXPECT_EQ(items[2].title(), u"item2");
  EXPECT_FLOAT_EQ(items[2].ranking(), 30.f);

  // The tab with a timestamp more than a day ago wasn't ranked.
  EXPECT_EQ(items[3].title(), u"item3");
  EXPECT_FLOAT_EQ(items[3].ranking(), std::numeric_limits<float>::max());
}

TEST(BirchRankerTest, RankWeatherItems_Morning) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 9 AM in the morning.
  base::Time now = TimeFromString("22 Feb 2024 9:00 UTC");
  BirchRanker ranker(now);
  ASSERT_TRUE(ranker.IsMorning());

  // Create a weather item.
  BirchWeatherItem item(u"Sunny", u"72", ui::ImageModel());
  std::vector<BirchWeatherItem> items = {item};

  ranker.RankWeatherItems(&items);

  ASSERT_EQ(1u, items.size());

  // The item had a ranking assigned.
  EXPECT_FLOAT_EQ(items[0].ranking(), 5.f);
}

TEST(BirchRankerTest, RankWeatherItems_Afternoon) {
  base::test::ScopedRestoreDefaultTimezone timezone("Etc/GMT");

  // Simulate 1 PM in the afternoon. Weather should not show.
  base::Time now = TimeFromString("22 Feb 2024 13:00 UTC");
  BirchRanker ranker(now);
  ASSERT_FALSE(ranker.IsMorning());

  // Create a weather item.
  BirchWeatherItem item(u"Sunny", u"72", ui::ImageModel());
  std::vector<BirchWeatherItem> items = {item};

  ranker.RankWeatherItems(&items);

  ASSERT_EQ(1u, items.size());

  // The item did not have a ranking assigned.
  EXPECT_FLOAT_EQ(items[0].ranking(), std::numeric_limits<float>::max());
}

}  // namespace
}  // namespace ash
