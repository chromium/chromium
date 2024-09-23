// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_metrics_recorder.h"

#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

namespace {
constexpr char kUserEmail[] = "user@example.com";
}  // namespace

class NightLightMetricsRecorderTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  NightLightControllerImpl* night_light_controller() {
    return Shell::Get()->night_light_controller();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(NightLightMetricsRecorderTest, DoNotRecordTemperature) {
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 0);

  SimulateUserLogin(kUserEmail);

  night_light_controller()->SetEnabled(false);
  night_light_controller()->SetScheduleType(ScheduleType::kNone);

  night_light_controller()->SetColorTemperature(0.52f);

  // Temperature should not be recorded if the Night Light is not enabled and
  // the ScheduleType is None.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 0);
}

TEST_F(NightLightMetricsRecorderTest,
       RecordTemperatureIfNightLightIsEnabledButScheduleTypeIsNone) {
  // Login so that prefs can be saved.
  SimulateUserLogin(kUserEmail);

  // No histograms should have been recorded yet.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 0);

  night_light_controller()->SetEnabled(true);
  night_light_controller()->SetScheduleType(ScheduleType::kNone);

  const float temperature = 0.11f;
  night_light_controller()->SetColorTemperature(temperature);

  // Simulate a sign out.
  Shell::Get()->session_controller()->RequestSignOut();

  // Now that prefs have been saved, login again.
  SimulateUserLogin(kUserEmail);

  // Temperature should be recorded if the Night Light is enabled, even though
  // the ScheduleType is None.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 1);
  histogram_tester_->ExpectUniqueSample("Ash.NightLight.Temperature.Initial",
                                        temperature * 100, 1);
}

TEST_F(NightLightMetricsRecorderTest,
       RecordTemperatureIfHasCustomScheduleButNightLightIsNotEnabled) {
  // Login so that prefs can be saved.
  SimulateUserLogin(kUserEmail);

  // No histograms should have been recorded yet.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 0);

  night_light_controller()->SetEnabled(false);
  night_light_controller()->SetScheduleType(ScheduleType::kCustom);

  const float temperature = 0.22f;
  night_light_controller()->SetColorTemperature(temperature);

  // Simulate a sign out.
  Shell::Get()->session_controller()->RequestSignOut();

  // Now that prefs have been saved, login again.
  SimulateUserLogin(kUserEmail);

  // Temperature should be recorded if the ScheduleType is Custom, even though
  // the Night Light is not enabled.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 1);
  histogram_tester_->ExpectUniqueSample("Ash.NightLight.Temperature.Initial",
                                        temperature * 100, 1);
}

TEST_F(NightLightMetricsRecorderTest,
       RecordTemperatureIfHasSunScheduleButNightLightIsNotEnabled) {
  // Login so that prefs can be saved.
  SimulateUserLogin(kUserEmail);

  // No histograms should have been recorded yet.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 0);

  night_light_controller()->SetEnabled(false);
  night_light_controller()->SetScheduleType(ScheduleType::kSunsetToSunrise);

  const float temperature = 0.33f;
  night_light_controller()->SetColorTemperature(temperature);

  // Simulate a sign out.
  Shell::Get()->session_controller()->RequestSignOut();

  // Now that prefs have been saved, login again.
  SimulateUserLogin(kUserEmail);

  // Temperature should be recorded if the ScheduleType is SunsetToSunrise,
  // even though the Night Light is not enabled.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.Temperature.Initial", 1);
  histogram_tester_->ExpectUniqueSample("Ash.NightLight.Temperature.Initial",
                                        temperature * 100, 1);
}

class NightLightMetricsRecorderTest_RecordScheduleType
    : public NightLightMetricsRecorderTest,
      public testing::WithParamInterface<ScheduleType> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NightLightMetricsRecorderTest_RecordScheduleType,
    testing::ValuesIn<ScheduleType>({
        ScheduleType::kNone,
        ScheduleType::kCustom,
        ScheduleType::kSunsetToSunrise,
    }));

TEST_P(NightLightMetricsRecorderTest_RecordScheduleType, RecordScheduleType) {
  // No histograms should have been recorded yet.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.ScheduleType.Initial", 0);

  // Login so that prefs can be saved.
  SimulateUserLogin(kUserEmail);

  // After first login, since the default ScheduleType is None, a histogram
  // should be recorded with that value.
  histogram_tester_->ExpectTotalCount("Ash.NightLight.ScheduleType.Initial", 1);
  histogram_tester_->ExpectUniqueSample("Ash.NightLight.ScheduleType.Initial",
                                        ScheduleType::kNone, 1);

  night_light_controller()->SetScheduleType(GetParam());

  // Simulate a sign out.
  Shell::Get()->session_controller()->RequestSignOut();

  // Now that prefs have been saved, login again.
  SimulateUserLogin(kUserEmail);

  histogram_tester_->ExpectTotalCount("Ash.NightLight.ScheduleType.Initial", 2);
  // When the ScheduleType is None, an additional sample is expected since it
  // was recorded once earlier.
  const int number_of_expected_samples =
      GetParam() == ScheduleType::kNone ? 2 : 1;
  histogram_tester_->ExpectBucketCount(
      "Ash.NightLight.ScheduleType.Initial", /*sample=*/GetParam(),
      /*expected_count=*/number_of_expected_samples);
}

}  // namespace ash
