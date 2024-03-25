// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_accessible_alerts_map.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using Alert = DownloadBubbleAccessibleAlertsMap::Alert;
using offline_items_collection::ContentId;
using Urgency = Alert::Urgency;

ContentId CreateTestContentId(const std::string& id) {
  return ContentId{"test", id};
}

class DownloadBubbleAccessibleAlertsMapTest : public ::testing::Test {
 public:
  DownloadBubbleAccessibleAlertsMap& map() { return map_; }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  DownloadBubbleAccessibleAlertsMap map_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(DownloadBubbleAccessibleAlertsMapTest, AddNormalAlertToEmptyMap) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(Pair(
                  CreateTestContentId("download1"),
                  AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                        Field(&Alert::text, u"alert1")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest, AddUrgentAlertToEmptyMap) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddTwoAlertsForDifferentDownloads) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert1"}));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download2"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert2"}));
  EXPECT_THAT(
      map().unannounced_alerts_for_testing(),
      UnorderedElementsAre(
          Pair(CreateTestContentId("download1"),
               AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                     Field(&Alert::text, u"alert1"))),
          Pair(CreateTestContentId("download2"),
               AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                     Field(&Alert::text, u"alert2")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1", u"alert2"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now),
                           Pair(CreateTestContentId("download2"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddTwoNormalAlertsForSameDownload) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert1"}));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(Pair(
                  CreateTestContentId("download1"),
                  AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                        Field(&Alert::text, u"alert2")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert2"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddTwoUrgentAlertsForSameDownload) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert2")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert2"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddNormalThenUrgentForSameDownload) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert1"}));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert2")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert2"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddUrgentThenNormalForSameDownload) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_FALSE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       AddTwoAlertsForSameDownload_BackwardsTime) {
  struct {
    Alert::Urgency first_alert_urgency;
    Alert::Urgency second_alert_urgency;
  } kTestCases[] = {
      {Urgency::kAlertSoon, Urgency::kAlertSoon},
      {Urgency::kAlertSoon, Urgency::kAlertWhenAppropriate},
      {Urgency::kAlertWhenAppropriate, Urgency::kAlertSoon},
      {Urgency::kAlertWhenAppropriate, Urgency::kAlertWhenAppropriate},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(base::StringPrintf(
        "first: %d second: %d", static_cast<int>(test_case.first_alert_urgency),
        static_cast<int>(test_case.second_alert_urgency)));
    // Make a new map so test cases are independent.
    DownloadBubbleAccessibleAlertsMap map;

    // Simulate system clock jumping backwards (base::Time is not guaranteed to
    // be monotonic), by creating the second alert before creating the first.
    Alert alert2{test_case.second_alert_urgency, u"alert2"};
    FastForwardBy(base::Seconds(30));
    Alert alert1{test_case.first_alert_urgency, u"alert1"};
    EXPECT_TRUE(map.MaybeAddAccessibleAlert(CreateTestContentId("download1"),
                                            std::move(alert1)));
    EXPECT_FALSE(map.MaybeAddAccessibleAlert(CreateTestContentId("download1"),
                                             std::move(alert2)));

    // The first-added alert should be retained because it is "more recent" than
    // the new one.
    EXPECT_THAT(map.unannounced_alerts_for_testing(),
                UnorderedElementsAre(Pair(
                    CreateTestContentId("download1"),
                    AllOf(Field(&Alert::urgency, test_case.first_alert_urgency),
                          Field(&Alert::text, u"alert1")))));
  }
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest, DontAnnounceStaleAlert) {
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));
  // Advance time more than 1 minute to make the alert stale.
  FastForwardBy(base::Minutes(2));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, IsEmpty());
  EXPECT_THAT(map().last_alerted_times_for_testing(), IsEmpty());
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest,
       DontAnnounceAlreadyAnnouncedTooSoon) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
  // Advance time less than 3 minutes.
  FastForwardBy(base::Minutes(2));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(Pair(
                  CreateTestContentId("download1"),
                  AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                        Field(&Alert::text, u"alert2")))));
  to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, IsEmpty());
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(Pair(
                  CreateTestContentId("download1"),
                  AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                        Field(&Alert::text, u"alert2")))));
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest, AnnounceAfterIntervalPassed) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert1"));
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(Pair(CreateTestContentId("download1"), now)));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
  // Advance time more than 3 minutes.
  FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"),
      Alert{Urgency::kAlertWhenAppropriate, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(Pair(
                  CreateTestContentId("download1"),
                  AllOf(Field(&Alert::urgency, Urgency::kAlertWhenAppropriate),
                        Field(&Alert::text, u"alert2")))));
  to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert2"));
  EXPECT_THAT(map().last_alerted_times_for_testing(),
              UnorderedElementsAre(Pair(CreateTestContentId("download1"),
                                        now + base::Minutes(4))));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
}

TEST_F(DownloadBubbleAccessibleAlertsMapTest, GarbageCollect) {
  base::Time now = base::Time::Now();
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download1"), Alert{Urgency::kAlertSoon, u"alert1"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1")))));

  // Advance time more than 1 minute to make the first alert stale.
  FastForwardBy(base::Minutes(2));
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download2"), Alert{Urgency::kAlertSoon, u"alert2"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download1"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert1"))),
                  Pair(CreateTestContentId("download2"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert2")))));

  // Garbage collecting should delete the stale alert.
  map().GarbageCollect();
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download2"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert2")))));

  // Announce the second alert to make a last-alerted-time appear for alert2.
  std::vector<std::u16string> to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert2"));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
  EXPECT_THAT(map().last_alerted_times_for_testing(),
              UnorderedElementsAre(Pair(CreateTestContentId("download2"),
                                        now + base::Minutes(2))));

  // Advance time by more than 6 minutes to make alert2's time stale.
  FastForwardBy(base::Minutes(7));

  // Add another alert that will not be stale.
  EXPECT_TRUE(map().MaybeAddAccessibleAlert(
      CreateTestContentId("download3"), Alert{Urgency::kAlertSoon, u"alert3"}));
  EXPECT_THAT(map().unannounced_alerts_for_testing(),
              UnorderedElementsAre(
                  Pair(CreateTestContentId("download3"),
                       AllOf(Field(&Alert::urgency, Urgency::kAlertSoon),
                             Field(&Alert::text, u"alert3")))));
  EXPECT_THAT(map().last_alerted_times_for_testing(),
              UnorderedElementsAre(Pair(CreateTestContentId("download2"),
                                        now + base::Minutes(2))));
  // Announce the third alert to make a last-alerted-time appear for alert3.
  to_announce = map().TakeAlertsForAnnouncement();
  EXPECT_THAT(to_announce, UnorderedElementsAre(u"alert3"));
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
  EXPECT_THAT(
      map().last_alerted_times_for_testing(),
      UnorderedElementsAre(
          Pair(CreateTestContentId("download2"), now + base::Minutes(2)),
          Pair(CreateTestContentId("download3"), now + base::Minutes(2 + 7))));

  // Garbage collecting should delete the stale time.
  map().GarbageCollect();
  EXPECT_THAT(map().unannounced_alerts_for_testing(), IsEmpty());
  EXPECT_THAT(map().last_alerted_times_for_testing(),
              UnorderedElementsAre(Pair(CreateTestContentId("download3"),
                                        now + base::Minutes(2 + 7))));
}

}  // namespace
