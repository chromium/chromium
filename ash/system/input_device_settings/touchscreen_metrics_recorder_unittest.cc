// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/touchscreen_metrics_recorder.h"

#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace ash {

class TouchscreenMetricsRecorderTest : public AshTestBase {
 public:
  TouchscreenMetricsRecorderTest() = default;
  TouchscreenMetricsRecorderTest(const TouchscreenMetricsRecorderTest&) =
      delete;
  TouchscreenMetricsRecorderTest& operator=(
      const TouchscreenMetricsRecorderTest&) = delete;
  ~TouchscreenMetricsRecorderTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

  void TearDown() override { AshTestBase::TearDown(); }
};

TEST_F(TouchscreenMetricsRecorderTest, RecordTouchscreenConfiguration) {
  base::HistogramTester histogram_tester;

  ui::TouchscreenDevice internal_touchdevice(
      10, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("internal touch device"), gfx::Size(1000, 1000), 1);
  ui::TouchscreenDevice external_touchdevice(
      11, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("external touch device"), gfx::Size(1000, 1000), 1);

  // Test with zero touchscreen.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Inputs.Touchscreen.Connected.External.Count", 0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Inputs.Touchscreen.Connected.Configuration", 0);

  // Test with one internal touchscreen.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({internal_touchdevice});

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Inputs.Touchscreen.Connected.External.Count", 0);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Touchscreen.Connected.Configuration",
      TouchscreenConfiguration::InternalOneExternalNone, 1);

  // Test with one external touchscren.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({external_touchdevice});
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ChromeOS.Inputs.Touchscreen.Connected.External.Count"),
              testing::ElementsAre(base::Bucket(1, 1)));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Touchscreen.Connected.Configuration",
      TouchscreenConfiguration::InternalNoneExternalOne, 1);

  // Test with one internal and one external touchscreen.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {internal_touchdevice, external_touchdevice});

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ChromeOS.Inputs.Touchscreen.Connected.External.Count"),
              testing::ElementsAre(base::Bucket(1, 2)));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Touchscreen.Connected.Configuration",
      TouchscreenConfiguration::InternalOneExternalOne, 1);
}

}  // namespace ash
