// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/views/controls/slider.h"

namespace ash {

class UnifiedVolumeSliderControllerTest : public AshTestBase {
 public:
  UnifiedVolumeSliderControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  UnifiedVolumeSliderControllerTest(const UnifiedVolumeSliderControllerTest&) =
      delete;
  UnifiedVolumeSliderControllerTest& operator=(
      const UnifiedVolumeSliderControllerTest&) = delete;

  ~UnifiedVolumeSliderControllerTest() override = default;

 protected:
  void UpdateSliderValue(float new_value) {
    unified_volume_slider_controller_.SliderValueChanged(
        /*sender=*/nullptr, new_value,
        /*old_value=*/0, views::SliderChangeReason::kByUser);
  }

  base::HistogramTester histogram_tester_;

 private:
  UnifiedVolumeSliderController unified_volume_slider_controller_;
};

// Verify moving the slider and changing the output volume is recorded to
// metrics.
TEST_F(UnifiedVolumeSliderControllerTest, RecordOutputVolumeChangedSource) {
  // Move the slider 3 times. Move the slider at half of the delay interval
  // time so each change shouldn't be recorded.
  UpdateSliderValue(/*new_value=*/10);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  UpdateSliderValue(/*new_value=*/20);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 0);

  UpdateSliderValue(/*new_value=*/30);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);

  // Move the slider up 2 times. Move the slider at half of the delay interval
  // time so each change shouldn't be recorded.
  UpdateSliderValue(/*new_value=*/50);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);

  UpdateSliderValue(/*new_value=*/100);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 2);
}

}  // namespace ash
