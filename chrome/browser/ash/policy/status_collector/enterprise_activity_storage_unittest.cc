// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/enterprise_activity_storage.h"

#include <memory>

#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::AllOf;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

namespace em = ::enterprise_management;

const char kPrefName[] = "pref-name";

}  // namespace

class EnterpriseActivityStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    local_state_.registry()->RegisterDictionaryPref(kPrefName);
    storage_ =
        std::make_unique<EnterpriseActivityStorage>(&local_state_, kPrefName);
  }

  static testing::Matcher<em::TimePeriod> EqActivity(
      const base::Time& start_time,
      const base::Time& end_time) {
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

  EnterpriseActivityStorage* storage() { return storage_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<EnterpriseActivityStorage> storage_;
};

TEST_F(EnterpriseActivityStorageTest, GetFilteredActivityPeriods) {
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 11:30pm"),
                               MakeLocalTime("28-MAR-2020 11:45pm"),
                               "user0@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 3:30pm"),
                               MakeLocalTime("29-MAR-2020 8:30pm"),
                               "user0@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 1:30am"),
                               MakeLocalTime("28-MAR-2020 8:30pm"),
                               "user1@managed.com");

  auto activity_periods = storage()->GetFilteredActivityPeriods(false);
  EXPECT_THAT(
      activity_periods["user0@managed.com"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 12:15am")),
                           EqActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 5:00am"))));
  EXPECT_THAT(
      activity_periods["user1@managed.com"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 7:00pm"))));

  auto redacted_activity_periods = storage()->GetFilteredActivityPeriods(true);
  EXPECT_THAT(
      redacted_activity_periods[""],
      UnorderedElementsAre(EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 7:15pm")),
                           EqActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 5:00am"))));
}

TEST_F(EnterpriseActivityStorageTest, FilterActivityPeriodsByUsers) {
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 11:30pm"),
                               MakeLocalTime("28-MAR-2020 11:45pm"),
                               "user0@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("28-MAR-2020 1:30am"),
                               MakeLocalTime("28-MAR-2020 8:30pm"),
                               "user1@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 3:30pm"),
                               MakeLocalTime("29-MAR-2020 8:30pm"),
                               "user0@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("29-MAR-2020 1:30am"),
                               MakeLocalTime("29-MAR-2020 2:30pm"),
                               "user2@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("30-MAR-2020 1:00am"),
                               MakeLocalTime("30-MAR-2020 1:45am"),
                               "user0@managed.com");
  storage()->AddActivityPeriod(MakeLocalTime("30-MAR-2020 2:00am"),
                               MakeLocalTime("30-MAR-2020 2:55am"));

  std::vector<std::string> reporting_users{"user2@managed.com"};
  storage()->FilterActivityPeriodsByUsers(reporting_users);

  auto redacted_activity_periods = storage()->GetFilteredActivityPeriods(false);
  EXPECT_THAT(
      redacted_activity_periods["user2@managed.com"],
      UnorderedElementsAre(EqActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 1:00pm"))));
  EXPECT_THAT(
      redacted_activity_periods[""],
      UnorderedElementsAre(EqActivity(MakeUTCTime("28-MAR-2020 12:00am"),
                                      MakeUTCTime("28-MAR-2020 7:15pm")),
                           EqActivity(MakeUTCTime("29-MAR-2020 12:00am"),
                                      MakeUTCTime("29-MAR-2020 5:00am")),
                           EqActivity(MakeUTCTime("30-MAR-2020 12:00am"),
                                      MakeUTCTime("30-MAR-2020 1:40am"))));
}

}  // namespace policy
