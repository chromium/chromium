// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class FakeDelegate : public UnifiedVolumeSliderController::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  void OnAudioSettingsButtonClicked() override {}
};

class UnifiedVolumeSliderControllerTest : public AshTestBase {
 public:
  UnifiedVolumeSliderControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  UnifiedVolumeSliderControllerTest(const UnifiedVolumeSliderControllerTest&) =
      delete;
  UnifiedVolumeSliderControllerTest& operator=(
      const UnifiedVolumeSliderControllerTest&) = delete;

  ~UnifiedVolumeSliderControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<FakeDelegate>();
    unified_volume_slider_controller_ =
        std::make_unique<UnifiedVolumeSliderController>(delegate_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    slider_view_ = unified_volume_slider_controller_->CreateView();
    widget_->SetContentsView(slider_view_.get());
  }

  void TearDown() override {
    slider_view_.reset();
    widget_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void UpdateSliderValue(float new_value) {
    unified_volume_slider_controller_->SliderValueChanged(
        /*sender=*/nullptr, new_value,
        /*old_value=*/0, views::SliderChangeReason::kByUser);
  }

  void PressSliderButton() {
    LeftClickOn(static_cast<UnifiedVolumeView*>(slider_view_.get())->button());
  }

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<UnifiedVolumeSliderController>
      unified_volume_slider_controller_;
  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<views::View> slider_view_;
  std::unique_ptr<views::Widget> widget_;
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
