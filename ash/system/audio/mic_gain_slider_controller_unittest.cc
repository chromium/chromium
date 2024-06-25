// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_controller.h"

#include <memory>

#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/views/controls/slider.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MicGainSliderControllerTest : public AshTestBase {
 public:
  MicGainSliderControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MicGainSliderControllerTest(const MicGainSliderControllerTest&) = delete;
  MicGainSliderControllerTest& operator=(const MicGainSliderControllerTest&) =
      delete;

  ~MicGainSliderControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    slider_view_ = mic_gain_slider_controller_.CreateView();
    widget_->SetContentsView(slider_view_.get());
  }

  void TearDown() override {
    slider_view_.reset();
    widget_.reset();
    AshTestBase::TearDown();
  }

  views::View* GetMuteToastView() { return slider_view_.get(); }

 protected:
  void UpdateSliderValue(float new_value) {
    mic_gain_slider_controller_.SliderValueChanged(
        /*sender=*/nullptr, new_value,
        /*old_value=*/0, views::SliderChangeReason::kByUser);
  }

  void PressSliderButton() {
    LeftClickOn(static_cast<MicGainSliderView*>(slider_view_.get())->button());
  }

  base::HistogramTester histogram_tester_;

 private:
  MicGainSliderController mic_gain_slider_controller_;
  std::unique_ptr<views::View> slider_view_;
  std::unique_ptr<views::Widget> widget_;
};

// Verify moving the slider and changing the gain is recorded to metrics.
TEST_F(MicGainSliderControllerTest, RecordInputGainChangedSource) {
  // Move the slider 3 times. Move the slider at half of the delay interval
  // time so each change shouldn't be recorded.
  UpdateSliderValue(/*new_value=*/10);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  UpdateSliderValue(/*new_value=*/20);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 0);

  UpdateSliderValue(/*new_value=*/30);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);

  // Move the slider up 2 times. Move the slider at half of the delay interval
  // time so each change shouldn't be recorded.
  UpdateSliderValue(/*new_value=*/50);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval / 2);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);

  UpdateSliderValue(/*new_value=*/100);
  task_environment()->FastForwardBy(
      CrasAudioHandler::kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 2);
}

TEST_F(MicGainSliderControllerTest, CreateMuteToastView) {
  auto* toast_view = GetMuteToastView();
  // `MicGainSliderView` is the first child in the toast view and is visible.
  EXPECT_TRUE(toast_view->children()[0]->GetVisible());
}

}  // namespace ash
