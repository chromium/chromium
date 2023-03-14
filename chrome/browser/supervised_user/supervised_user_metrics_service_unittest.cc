// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_metrics_service.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kStartTime[] = "1 Jan 2020 21:00";

}  // namespace

// Tests for family user metrics service.
class SupervisedUserMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);

    supervised_user_metrics_service_ =
        std::make_unique<SupervisedUserMetricsService>(&testing_profile_);
  }

  void TearDown() override {
    supervised_user_metrics_service_->Shutdown();
    supervised_user_metrics_service_.reset();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return testing_profile_.GetTestingPrefService();
  }

  int GetDayIdPref() {
    return GetPrefService()->GetInteger(prefs::kSupervisedUserMetricsDayId);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingProfile testing_profile_;
  std::unique_ptr<SupervisedUserMetricsService>
      supervised_user_metrics_service_;
};

// Tests OnNewDay() is called after more than one day passes.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMultipleDays) {
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAtMidnight) {
  task_environment_.FastForwardBy(base::Hours(3));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(SupervisedUserMetricsServiceTest, NewDayAfterMidnight) {
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(SupervisedUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}
