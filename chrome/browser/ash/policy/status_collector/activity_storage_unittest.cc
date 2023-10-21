// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/activity_storage.h"

#include <memory>

#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

namespace em = enterprise_management;

namespace {
constexpr char kPrefName[] = "pref-name";
}  // namespace

namespace policy {

class ActivityStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    local_state_.registry()->RegisterDictionaryPref(kPrefName);
    storage_ = std::make_unique<ActivityStorage>(&local_state_, kPrefName,
                                                 base::Days(0));
  }

  static testing::Matcher<em::TimePeriod> EqActivity(
      const base::Time start_time,
      const base::Time end_time) {
    return AllOf(Property(&em::TimePeriod::start_timestamp,
                          start_time.InMillisecondsSinceUnixEpoch()),
                 Property(&em::TimePeriod::end_timestamp,
                          end_time.InMillisecondsSinceUnixEpoch()));
  }

  base::Time MakeLocalTime(const std::string& time_string) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_string.c_str(), &time));
    return time;
  }

  base::Time MakeUTCTime(const std::string& time_string) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromUTCString(time_string.c_str(), &time));
    return time;
  }

  ActivityStorage* storage() { return storage_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ActivityStorage> storage_;
};

TEST_F(ActivityStorageTest, GetBeginningOfDay) {
  const base::Time& time = MakeLocalTime("28-MAR-2020 11:30pm");

  const base::Time& day_beginning = storage()->GetBeginningOfDay(time);

  EXPECT_EQ(day_beginning, MakeLocalTime("28-MAR-2020 12:00am"));
}

TEST_F(ActivityStorageTest, PruneActivityPeriods) {
  storage()->AddActivityPeriod(MakeLocalTime("25-MAR-2020 3:30pm"),
                               MakeLocalTime("25-MAR-2020 6:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("26-MAR-2020 3:00pm"),
                               MakeLocalTime("26-MAR-2020 7:00pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("27-MAR-2020 2:30pm"),
                               MakeLocalTime("27-MAR-2020 7:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 2:00pm"),
                               MakeLocalTime("28-MAR-2020 8:00pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 1:30pm"),
                               MakeLocalTime("29-MAR-2020 8:30pm"), "id1");

  storage()->PruneActivityPeriods(MakeLocalTime("27-MAR-2020 3:30pm"),
                                  base::Days(1), base::Days(2));

  auto activity_periods = storage()->GetActivityPeriods();
  EXPECT_THAT(
      activity_periods["id1"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("26-MAR-2020 12:00am"),
                                      MakeUTCTime("26-MAR-2020 4:00am")),
                           EqActivity(MakeUTCTime("27-MAR-2020 12:00am"),
                                      MakeUTCTime("27-MAR-2020 5:00am")),
                           EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 6:00am"))));
}

TEST_F(ActivityStorageTest, TrimActivityPeriods) {
  storage()->AddActivityPeriod(MakeLocalTime("25-MAR-2020 3:30pm"),
                               MakeLocalTime("25-MAR-2020 6:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("26-MAR-2020 3:00pm"),
                               MakeLocalTime("26-MAR-2020 7:00pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("27-MAR-2020 2:30pm"),
                               MakeLocalTime("27-MAR-2020 7:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 2:00pm"),
                               MakeLocalTime("28-MAR-2020 8:00pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 1:30pm"),
                               MakeLocalTime("29-MAR-2020 8:30pm"), "id1");

  storage()->TrimActivityPeriods(
      MakeUTCTime("26-MAR-2020 3:00am").InMillisecondsSinceUnixEpoch(),
      MakeUTCTime("28-MAR-2020 2:00am").InMillisecondsSinceUnixEpoch());

  auto activity_periods = storage()->GetActivityPeriods();
  EXPECT_THAT(
      activity_periods["id1"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("26-MAR-2020 12:00am"),
                                      MakeUTCTime("26-MAR-2020 1:00am")),
                           EqActivity(MakeUTCTime("27-MAR-2020 12:00am"),
                                      MakeUTCTime("27-MAR-2020 5:00am")),
                           EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 2:00am"))));
}

TEST_F(ActivityStorageTest, RemoveOverlappingActivityPeriods) {
  storage()->AddActivityPeriod(MakeLocalTime("26-MAR-2020 1:30pm"),
                               MakeLocalTime("26-MAR-2020 8:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("26-MAR-2020 1:30am"),
                               MakeLocalTime("26-MAR-2020 8:30pm"), "id2");

  storage()->RemoveOverlappingActivityPeriods();

  auto activity_periods = storage()->GetActivityPeriods();
  EXPECT_THAT(activity_periods["id1"], UnorderedElementsAre(EqActivity(
                                           MakeUTCTime("26-MAR-2020 12:00am"),
                                           MakeUTCTime("26-MAR-2020 7:00am"))));
  EXPECT_THAT(activity_periods["id2"], UnorderedElementsAre(EqActivity(
                                           MakeUTCTime("26-MAR-2020 12:00am"),
                                           MakeUTCTime("26-MAR-2020 5:00pm"))));
}

TEST_F(ActivityStorageTest, GetActivityPeriodsWithNoId) {
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 4:30pm"),
                               MakeLocalTime("28-MAR-2020 11:30pm"));
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 3:30pm"),
                               MakeLocalTime("29-MAR-2020 8:30pm"), "id1");

  auto activity_periods = storage()->GetActivityPeriodsWithNoId();
  EXPECT_THAT(activity_periods, UnorderedElementsAre(EqActivity(
                                    MakeUTCTime("28-MAR-2020 12:00am"),
                                    MakeUTCTime("28-MAR-2020 7:00am"))));
}

TEST_F(ActivityStorageTest, AddActivityPeriod) {
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 11:30pm"),
                               MakeLocalTime("28-MAR-2020 11:45pm"), "id2");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 3:30pm"),
                               MakeLocalTime("31-MAR-2020 8:30pm"), "id1");
  storage()->AddActivityPeriod(MakeLocalTime("26-MAR-2020 1:30am"),
                               MakeLocalTime("26-MAR-2020 8:30pm"), "id2");

  auto activity_periods = storage()->GetActivityPeriods();
  EXPECT_THAT(
      activity_periods["id1"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 8:30am")),
                           EqActivity(MakeUTCTime("30-MAR-2020 12:00am"),
                                      MakeUTCTime("31-MAR-2020 12:00am")),
                           EqActivity(MakeUTCTime("31-MAR-2020 12:00am"),
                                      MakeUTCTime("31-MAR-2020 8:30pm"))));
  EXPECT_THAT(
      activity_periods["id2"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("26-MAR-2020 12:00am"),
                                      MakeUTCTime("26-MAR-2020 7:00pm")),
                           EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 12:15am"))));
}

}  // namespace policy
