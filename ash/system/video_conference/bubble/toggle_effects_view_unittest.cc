// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash::video_conference {

namespace {
constexpr char kTestEffectHistogramName[] =
    "Ash.VideoConferenceTray.TestEffect.Click";
}  // namespace

class ToggleEffectsViewTest : public AshTestBase {
 public:
  ToggleEffectsViewTest() = default;
  ToggleEffectsViewTest(const ToggleEffectsViewTest&) = delete;
  ToggleEffectsViewTest& operator=(const ToggleEffectsViewTest&) = delete;
  ~ToggleEffectsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Instantiates a fake controller (the real one is created in
    // `ChromeBrowserMainExtraPartsAsh::PreProfileInit()` which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    office_bunny_ =
        std::make_unique<fake_video_conference::OfficeBunnyEffect>();

    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    office_bunny_.reset();
    controller_.reset();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  views::View* bubble_view() {
    return video_conference_tray()->GetBubbleView();
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  views::View* toggle_effect_button() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsButton);
  }

  ash::fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<ash::fake_video_conference::OfficeBunnyEffect> office_bunny_;
};

TEST_F(ToggleEffectsViewTest, ToggleButtonClickedRecordedHistogram) {
  base::HistogramTester histogram_tester;

  // Adds one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Clicks to open the bubble, toggle effect button is present/visible.
  LeftClickOn(toggle_bubble_button());
  ASSERT_TRUE(toggle_effect_button());
  ASSERT_TRUE(toggle_effect_button()->GetVisible());

  // Clicks the toggle effect button, verify that metrics is recorded.
  LeftClickOn(toggle_effect_button());
  histogram_tester.ExpectBucketCount(kTestEffectHistogramName, true, 1);

  // Clicks again.
  LeftClickOn(toggle_effect_button());
  histogram_tester.ExpectBucketCount(kTestEffectHistogramName, false, 1);
}

}  // namespace ash::video_conference