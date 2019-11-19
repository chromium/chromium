// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_reporting_util.h"

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;

namespace crostini {

class CrostiniReportingUtilTest : public testing::Test {
 public:
  CrostiniReportingUtilTest() = default;

 protected:
  void enable_crostini_reporting() {
    profile_.GetPrefs()->SetBoolean(prefs::kReportCrostiniUsageEnabled, true);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  TestingProfile profile_;
  component_updater::MockComponentUpdateService update_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniReportingUtilTest);
};

TEST_F(CrostiniReportingUtilTest, WriteMetricsForReportingToPrefsIfEnabled) {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 11:50:50 GMT", &time));
  test_clock_.SetNow(time);

  const auto component_info = component_updater::ComponentInfo(
      "id2", "fingerprint2", base::ASCIIToUTF16("cros-termina"),
      base::Version("1.33.7"));
  EXPECT_CALL(update_service_, GetComponents())
      .Times(1)
      .WillOnce(Return(
          std::vector<component_updater::ComponentInfo>({component_info})));

  PrefService* const preferences = profile_.GetPrefs();

  // Usage reporting is disabled by default, so we expect no usage logging,
  // which means no pref path exists and the prefs have default values:
  WriteMetricsForReportingToPrefsIfEnabled(preferences, &update_service_,
                                           &test_clock_);

  int64_t timestamp =
      preferences->GetInt64(prefs::kCrostiniLastLaunchTimeWindowStart);
  std::string termina_version =
      preferences->GetString(prefs::kCrostiniLastLaunchTerminaComponentVersion);
  EXPECT_FALSE(
      preferences->HasPrefPath(prefs::kCrostiniLastLaunchTimeWindowStart));
  EXPECT_FALSE(preferences->HasPrefPath(
      prefs::kCrostiniLastLaunchTerminaComponentVersion));
  EXPECT_EQ(0, timestamp);
  EXPECT_TRUE(termina_version.empty());

  // With usage reporting enabled, we should obtain non-default values:
  enable_crostini_reporting();

  WriteMetricsForReportingToPrefsIfEnabled(preferences, &update_service_,
                                           &test_clock_);

  timestamp = preferences->GetInt64(prefs::kCrostiniLastLaunchTimeWindowStart);
  termina_version =
      preferences->GetString(prefs::kCrostiniLastLaunchTerminaComponentVersion);
  EXPECT_EQ(1535760000000, timestamp);  // 1 Sep 2018 00:00:00 GMT
  EXPECT_EQ("1.33.7", termina_version);
}

TEST_F(CrostiniReportingUtilTest, WriteMetricsIfThereIsNoTerminaVersion) {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 11:50:50 GMT", &time));
  test_clock_.SetNow(time);
  PrefService* const preferences = profile_.GetPrefs();
  enable_crostini_reporting();

  // We test here that reporting does not break if no Termina version
  // can be found because the component is not registered under the
  // expected name.
  EXPECT_CALL(update_service_, GetComponents())
      .Times(1)
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>()));
  WriteMetricsForReportingToPrefsIfEnabled(preferences, &update_service_,
                                           &test_clock_);

  const int64_t timestamp =
      preferences->GetInt64(prefs::kCrostiniLastLaunchTimeWindowStart);
  const std::string termina_version =
      preferences->GetString(prefs::kCrostiniLastLaunchTerminaComponentVersion);
  EXPECT_EQ(1535760000000, timestamp);  // 1 Sep 2018 00:00:00 GMT
  EXPECT_TRUE(termina_version.empty());
}

TEST_F(CrostiniReportingUtilTest, GetThreeDayWindowStart) {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString("Fri, 31 Aug 2018 11:50:50 GMT", &time));
  test_clock_.SetNow(time);

  // Time is set to the beginning of the day of the start of a
  // three day window (for privacy reasons).
  base::Time window_start;
  EXPECT_TRUE(
      base::Time::FromString("Wed, 29 Aug 2018 00:00:00 GMT", &window_start));
  EXPECT_EQ(window_start, GetThreeDayWindowStart(test_clock_.Now()));

  // Since a three-day period has been crossed, another time is returned
  // for three consecutive days:
  base::Time next_window_start;
  EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 00:00:00 GMT",
                                     &next_window_start));
  test_clock_.Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(next_window_start, GetThreeDayWindowStart(test_clock_.Now()));

  test_clock_.Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(next_window_start, GetThreeDayWindowStart(test_clock_.Now()));

  test_clock_.Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(next_window_start, GetThreeDayWindowStart(test_clock_.Now()));

  // After three consecutive days logged with the same value, we now expect
  // a three day change again:
  base::Time three_days_later;
  EXPECT_TRUE(base::Time::FromString("Tue, 4 Sep 2018 00:00:00 GMT",
                                     &three_days_later));
  test_clock_.Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(three_days_later, GetThreeDayWindowStart(test_clock_.Now()));
}

TEST_F(CrostiniReportingUtilTest, GetTerminaVersion) {
  component_updater::MockComponentUpdateService* const update_service =
      &update_service_;
  EXPECT_CALL(*update_service, GetComponents())
      .Times(1)
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>()));
  EXPECT_TRUE(GetTerminaVersion(update_service).empty());

  Mock::VerifyAndClearExpectations(update_service);

  const auto component_info_1 = component_updater::ComponentInfo(
      "id1", "fingerprint1", base::ASCIIToUTF16("name1"), base::Version("1.0"));
  const auto component_info_2 = component_updater::ComponentInfo(
      "id2", "fingerprint2", base::ASCIIToUTF16("cros-termina"),
      base::Version("1.33.7"));
  const auto component_info_3 = component_updater::ComponentInfo(
      "id3", "fingerprint3", base::ASCIIToUTF16("name1"), base::Version("1.0"));
  EXPECT_CALL(*update_service, GetComponents())
      .Times(1)
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>(
          {component_info_1, component_info_2, component_info_3})));

  EXPECT_EQ("1.33.7", GetTerminaVersion(update_service));
}

}  // namespace crostini
