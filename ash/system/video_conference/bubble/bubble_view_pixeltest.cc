// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/image_view.h"

namespace ash::video_conference {

class BubbleViewPixelTest : public AshTestBase {
 public:
  BubbleViewPixelTest() = default;
  BubbleViewPixelTest(const BubbleViewPixelTest&) = delete;
  BubbleViewPixelTest& operator=(const BubbleViewPixelTest&) = delete;
  ~BubbleViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference, chromeos::features::kJelly}, {});
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

  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
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

  views::View* GetToggleEffectsView() {
    return bubble_view()->GetViewByID(BubbleViewID::kToggleEffectsView);
  }

  ash::fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<ash::fake_video_conference::OfficeBunnyEffect> office_bunny_;
};

// Pixel test that tests toggled on/off and focused/not focused for the toggle
// effect button.
TEST_F(BubbleViewPixelTest, ToggleButton) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button should be visible.
  LeftClickOn(video_conference_tray()->GetToggleBubbleButtonForTest());

  ASSERT_TRUE(bubble_view());
  auto* first_toggle_effect_button = GetFirstToggleEffectButton();
  ASSERT_TRUE(first_toggle_effect_button);

  // The bounds paint slightly outside of `first_toggle_effect_button`'s bounds,
  // so grab the scroll view's contents view. This is sterile for this pixel
  // test because the test effect (office bunny) only has a single toggle with
  // no sliders.
  auto* toggle_effect_button_container = GetToggleEffectsView()->parent();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_no_focus_not_toggled",
      /*revision_number=*/0, toggle_effect_button_container));

  // Toggle the first button, the UI should change.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(1, office_bunny()->num_activations_for_testing());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_no_focus_toggled",
      /*revision_number=*/0, toggle_effect_button_container));

  // Un-toggle the button, then keyboard focus it.
  LeftClickOn(first_toggle_effect_button);
  ASSERT_EQ(2, office_bunny()->num_activations_for_testing());
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_not_toggled",
      /*revision_number=*/0, toggle_effect_button_container));

  // Re-toggle the button.
  event_generator->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  ASSERT_EQ(3, office_bunny()->num_activations_for_testing());
  ASSERT_TRUE(first_toggle_effect_button->HasFocus());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "video_conference_bubble_view_with_focus_toggled",
      /*revision_number=*/0, toggle_effect_button_container));
}

}  // namespace ash::video_conference
