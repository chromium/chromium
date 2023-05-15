// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"

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

  // Each toggle button in the bubble view has this view ID, this just gets the
  // first one in the view tree.
  views::Button* GetFirstToggleEffectButton() {
    return static_cast<views::Button*>(bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsButton));
  }

  // Each toggle button icon in a toggle button has this view ID, this just gets
  // the first one in the view tree.
  views::ImageView* GetFirstToggleEffectIcon() {
    return static_cast<views::ImageView*>(bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectIcon));
  }

  ash::fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<ash::fake_video_conference::OfficeBunnyEffect> office_bunny_;
};

// Tests that a toggle button records histograms when clicked.
TEST_F(ToggleEffectsViewTest, ToggleButtonClickedRecordedHistogram) {
  base::HistogramTester histogram_tester;

  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(toggle_bubble_button());
  ASSERT_TRUE(GetFirstToggleEffectButton());
  ASSERT_TRUE(GetFirstToggleEffectButton()->GetVisible());

  // Click the toggle effect button, verify that metrics is recorded.
  LeftClickOn(GetFirstToggleEffectButton());
  histogram_tester.ExpectBucketCount(kTestEffectHistogramName, true, 1);

  // Click again.
  LeftClickOn(GetFirstToggleEffectButton());
  histogram_tester.ExpectBucketCount(kTestEffectHistogramName, false, 1);
}

// Tests that a toggled ToggleButton's image is updated.
TEST_F(ToggleEffectsViewTest, ToggleUpdatesImage) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());
  LeftClickOn(toggle_bubble_button());

  // Initially the fake toggle effects icon is set to the video conference
  // background blur off icon.
  EXPECT_STREQ(GetFirstToggleEffectIcon()
                   ->GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               kVideoConferenceBackgroundBlurOffIcon.name);

  // Toggle the button, the icon should change to the privacy indicators camera
  // icon.
  LeftClickOn(GetFirstToggleEffectButton());
  EXPECT_STREQ(GetFirstToggleEffectIcon()
                   ->GetImageModel()
                   .GetVectorIcon()
                   .vector_icon()
                   ->name,
               ash::kPrivacyIndicatorsCameraIcon.name);
}

// Tests that a toggled ToggleButton's tooltip is updated.
TEST_F(ToggleEffectsViewTest, TooltipIsUpdated) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());
  LeftClickOn(toggle_bubble_button());

  EXPECT_EQ(
      GetFirstToggleEffectButton()->GetTooltipText(),
      l10n_util::GetStringFUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
          l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
          l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));

  // Toggle it on, the tooltip should update.
  LeftClickOn(GetFirstToggleEffectButton());

  EXPECT_EQ(
      GetFirstToggleEffectButton()->GetTooltipText(),
      l10n_util::GetStringFUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
          l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
          l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON)));
}

}  // namespace ash::video_conference