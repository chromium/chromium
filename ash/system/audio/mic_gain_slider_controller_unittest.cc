// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/mic_gain_slider_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/views/controls/slider.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MicGainSliderControllerTest : public AshTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  MicGainSliderControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

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

  // TODO(b/305075031) clean up after the flag is removed.
  bool IsQsRevampEnabled() const { return true; }

  std::unique_ptr<views::View> GetMuteToastView() {
    return mic_gain_slider_controller_.CreateView();
  }

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
  base::test::ScopedFeatureList feature_list_;
  MicGainSliderController mic_gain_slider_controller_;
  std::unique_ptr<views::View> slider_view_;
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         MicGainSliderControllerTest,
                         testing::Bool());

// Verify moving the slider and changing the gain is recorded to metrics.
TEST_P(MicGainSliderControllerTest, RecordInputGainChangedSource) {
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

TEST_P(MicGainSliderControllerTest, CreateMuteToastView) {
  auto toast_view = GetMuteToastView();
  if (IsQsRevampEnabled()) {
    // `MicGainSliderView` is the first child in the toast view and is visible.
    EXPECT_TRUE(toast_view->children()[0]->GetVisible());
  } else {
    EXPECT_EQ(
        u"Toggle Mic. Mic is on, toggling will mute input.",
        static_cast<IconButton*>(toast_view->children()[0])->GetTooltipText());
  }
}

// Verify pressing the mute button is recorded to metrics.
TEST_P(MicGainSliderControllerTest, RecordInputGainMuteSource) {
  // For QsRevamp: `slider_view_` doesn't have a `button()`.
  if (IsQsRevampEnabled()) {
    return;
  }
  PressSliderButton();
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);
}

}  // namespace ash
