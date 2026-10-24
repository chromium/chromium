// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_startup_metrics.h"

#include <stdint.h>

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/beacons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

namespace {

using shell_integration::IS_DEFAULT;
using shell_integration::NOT_DEFAULT;
using shell_integration::OTHER_MODE_IS_DEFAULT;
using shell_integration::UNKNOWN_DEFAULT;

constexpr char kTransitionHistogram[] = "DefaultBrowser.StartupStateTransition";
constexpr char kDaysHistogram[] =
    "DefaultBrowser.StartupLostDefault.DaysSinceChange";

const base::Time kSessionStart =
    base::Time::FromSecondsSinceUnixEpoch(1'800'000'000);

DefaultBrowserBeacons Beacons(base::Time first_not_default,
                              base::Time last_was_default) {
  return {.last_was_default = last_was_default,
          .first_not_default = first_not_default};
}

StartupStateTransition Classify(
    const DefaultBrowserBeacons& beacons,
    shell_integration::DefaultWebClientState current) {
  return ClassifyStartupTransition(beacons, kSessionStart, current);
}

TEST(DefaultBrowserStartupMetricsClassifyTest, NoBeacons) {
  const DefaultBrowserBeacons beacons = Beacons(base::Time(), base::Time());
  EXPECT_EQ(Classify(beacons, IS_DEFAULT),
            StartupStateTransition::kNoPreviousState);
  EXPECT_EQ(Classify(beacons, NOT_DEFAULT),
            StartupStateTransition::kNoPreviousState);
}

TEST(DefaultBrowserStartupMetricsClassifyTest, PreviouslyDefault) {
  const DefaultBrowserBeacons beacons =
      Beacons(base::Time(), kSessionStart - base::Days(1));
  EXPECT_EQ(Classify(beacons, IS_DEFAULT),
            StartupStateTransition::kRemainedDefault);
  EXPECT_EQ(Classify(beacons, NOT_DEFAULT),
            StartupStateTransition::kLostDefault);
}

TEST(DefaultBrowserStartupMetricsClassifyTest, PreviouslyNotDefault) {
  const DefaultBrowserBeacons beacons =
      Beacons(kSessionStart - base::Days(1), kSessionStart - base::Days(2));
  EXPECT_EQ(Classify(beacons, IS_DEFAULT),
            StartupStateTransition::kBecameDefault);
  EXPECT_EQ(Classify(beacons, NOT_DEFAULT),
            StartupStateTransition::kRemainedNotDefault);
}

// A FirstNotDefault beacon written in this session was set by an earlier
// in-process check and does not describe the previous session.
TEST(DefaultBrowserStartupMetricsClassifyTest, FreshFirstNotDefaultIgnored) {
  const DefaultBrowserBeacons beacons =
      Beacons(kSessionStart + base::Seconds(1), kSessionStart - base::Days(1));
  EXPECT_EQ(Classify(beacons, NOT_DEFAULT),
            StartupStateTransition::kLostDefault);
  EXPECT_EQ(Classify(beacons, IS_DEFAULT),
            StartupStateTransition::kRemainedDefault);

  EXPECT_EQ(Classify(Beacons(kSessionStart + base::Seconds(1), base::Time()),
                     NOT_DEFAULT),
            StartupStateTransition::kNoPreviousState);
}

TEST(DefaultBrowserStartupMetricsClassifyTest, CurrentStateUnknown) {
  EXPECT_EQ(Classify(Beacons(base::Time(), base::Time()), UNKNOWN_DEFAULT),
            StartupStateTransition::kCurrentStateUnknown);
  EXPECT_EQ(Classify(Beacons(base::Time(), kSessionStart - base::Days(1)),
                     UNKNOWN_DEFAULT),
            StartupStateTransition::kCurrentStateUnknown);
}

TEST(DefaultBrowserStartupMetricsClassifyTest, OtherModeIsDefault) {
  EXPECT_EQ(
      Classify(Beacons(base::Time(), base::Time()), OTHER_MODE_IS_DEFAULT),
      StartupStateTransition::kOtherModeIsDefault);
  EXPECT_EQ(Classify(Beacons(kSessionStart - base::Days(1), base::Time()),
                     OTHER_MODE_IS_DEFAULT),
            StartupStateTransition::kOtherModeIsDefault);
}

class DefaultBrowserStartupMetricsRecordTest : public testing::Test {
 protected:
  void Record(StartupStateTransition transition, base::Time change_time) {
    RecordStartupStateTransition(transition, change_time, kSessionStart);
    histogram_tester_.ExpectUniqueSample(kTransitionHistogram, transition, 1);
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(DefaultBrowserStartupMetricsRecordTest, LostDefaultRecordsDays) {
  Record(StartupStateTransition::kLostDefault,
         kSessionStart - base::Days(12) - base::Hours(5));
  histogram_tester_.ExpectUniqueSample(kDaysHistogram, 12, 1);
}

TEST_F(DefaultBrowserStartupMetricsRecordTest,
       LostDefaultFutureChangeTimeClampsToZero) {
  Record(StartupStateTransition::kLostDefault, kSessionStart + base::Days(1));
  histogram_tester_.ExpectUniqueSample(kDaysHistogram, 0, 1);
}

TEST_F(DefaultBrowserStartupMetricsRecordTest, NullChangeTimeRecordsNoDays) {
  Record(StartupStateTransition::kLostDefault, base::Time());
  histogram_tester_.ExpectTotalCount(kDaysHistogram, 0);
}

TEST_F(DefaultBrowserStartupMetricsRecordTest,
       DaysNotRecordedForOtherTransitions) {
  Record(StartupStateTransition::kRemainedNotDefault,
         kSessionStart - base::Days(3));
  histogram_tester_.ExpectTotalCount(kDaysHistogram, 0);
}

class DefaultBrowserStartupMetricsRegistryTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  void CreateUserChoiceKey(std::wstring_view path) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, path.data(), KEY_WRITE),
              ERROR_SUCCESS);
  }

  // Writes the LastWasDefault beacon in the format used by
  // installer_util::Beacon::Update() so that it can be back-dated.
  void WriteLastWasDefaultBeacon(base::Time time) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER,
                         install_static::InstallDetails::Get()
                             .GetClientStateKeyPath()
                             .c_str(),
                         KEY_WOW64_32KEY | KEY_SET_VALUE),
              ERROR_SUCCESS);
    const int64_t value = time.ToDeltaSinceWindowsEpoch().InMicroseconds();
    ASSERT_EQ(
        key.WriteValue(L"LastWasDefault", &value, sizeof(value), REG_QWORD),
        ERROR_SUCCESS);
  }

  base::HistogramTester histogram_tester_;

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(DefaultBrowserStartupMetricsRegistryTest, ReadBeaconsAbsent) {
  const DefaultBrowserBeacons beacons = ReadDefaultBrowserBeacons();
  EXPECT_TRUE(beacons.last_was_default.is_null());
  EXPECT_TRUE(beacons.first_not_default.is_null());
}

TEST_F(DefaultBrowserStartupMetricsRegistryTest, ReadBeaconsAfterUpdate) {
  const base::Time before = base::Time::Now();
  installer_util::MakeLastWasDefaultBeacon()->Update();
  const base::Time after = base::Time::Now();

  DefaultBrowserBeacons beacons = ReadDefaultBrowserBeacons();
  EXPECT_GE(beacons.last_was_default, before - base::Minutes(1));
  EXPECT_LE(beacons.last_was_default, after + base::Minutes(1));
  EXPECT_TRUE(beacons.first_not_default.is_null());

  installer_util::MakeFirstNotDefaultBeacon()->Update();
  beacons = ReadDefaultBrowserBeacons();
  EXPECT_FALSE(beacons.first_not_default.is_null());
}

TEST_F(DefaultBrowserStartupMetricsRegistryTest, ChangeTimeNullWithoutKeys) {
  EXPECT_TRUE(GetDefaultBrowserChangeTime().is_null());
}

TEST_F(DefaultBrowserStartupMetricsRegistryTest, ChangeTimeIsLatestKeyWrite) {
  const base::Time before = base::Time::Now();
  ASSERT_NO_FATAL_FAILURE(CreateUserChoiceKey(kUserChoiceKeyPaths[1]));
  const base::Time after = base::Time::Now();

  const base::Time https_only = GetDefaultBrowserChangeTime();
  EXPECT_GE(https_only, before - base::Minutes(1));
  EXPECT_LE(https_only, after + base::Minutes(1));

  ASSERT_NO_FATAL_FAILURE(CreateUserChoiceKey(kUserChoiceKeyPaths[0]));
  EXPECT_GE(GetDefaultBrowserChangeTime(), https_only);
}

TEST_F(DefaultBrowserStartupMetricsRegistryTest,
       RecordWithoutBeaconsIsNoPreviousState) {
  RecordStartupStateTransition(ReadDefaultBrowserBeacons(), NOT_DEFAULT);

  histogram_tester_.ExpectUniqueSample(
      kTransitionHistogram, StartupStateTransition::kNoPreviousState, 1);
  histogram_tester_.ExpectTotalCount(kDaysHistogram, 0);
}

TEST_F(DefaultBrowserStartupMetricsRegistryTest, RecordLostDefaultWithDays) {
  ASSERT_NO_FATAL_FAILURE(
      WriteLastWasDefaultBeacon(base::Time::Now() - base::Days(1)));
  ASSERT_NO_FATAL_FAILURE(CreateUserChoiceKey(kUserChoiceKeyPaths[0]));

  RecordStartupStateTransition(ReadDefaultBrowserBeacons(), NOT_DEFAULT);

  histogram_tester_.ExpectUniqueSample(kTransitionHistogram,
                                       StartupStateTransition::kLostDefault, 1);
  histogram_tester_.ExpectUniqueSample(kDaysHistogram, 0, 1);
}

}  // namespace

}  // namespace default_browser
