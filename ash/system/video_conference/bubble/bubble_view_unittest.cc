// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash::video_conference {

class BubbleViewTest : public AshTestBase {
 public:
  BubbleViewTest() = default;
  BubbleViewTest(const BubbleViewTest&) = delete;
  BubbleViewTest& operator=(const BubbleViewTest&) = delete;
  ~BubbleViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVcControlsUi);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not do it in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    // Instantiate these fake effects, to be registered/unregistered as needed.
    office_bunny_ =
        std::make_unique<fake_video_conference::OfficeBunnyEffect>();
    shaggy_fur_ = std::make_unique<fake_video_conference::ShaggyFurEffect>();

    set_create_global_cras_audio_handler(false);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    office_bunny_.reset();
    shaggy_fur_.reset();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  views::View* GetSetValueEffectButton(int index) {
    // Map `index` to a `BubbleViewID`, for lookup.
    BubbleViewID id =
        static_cast<BubbleViewID>(index + BubbleViewID::kSetValueButtonMin);
    DCHECK_GE(id, BubbleViewID::kSetValueButtonMin);
    DCHECK_LE(id, BubbleViewID::kSetValueButtonMax);
    return bubble_view()->GetViewByID(id);
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

  views::View* toggle_effects_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsView);
  }

  views::View* set_value_effects_view() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kSetValueEffectsView);
  }

  views::View* return_to_app() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kReturnToApp);
  }

  views::View* toggle_effect_button() {
    return bubble_view()->GetViewByID(
        video_conference::BubbleViewID::kToggleEffectsButton);
  }

  ash::fake_video_conference::OfficeBunnyEffect* office_bunny() {
    return office_bunny_.get();
  }

  ash::fake_video_conference::ShaggyFurEffect* shaggy_fur() {
    return shaggy_fur_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<ash::fake_video_conference::OfficeBunnyEffect> office_bunny_;
  std::unique_ptr<ash::fake_video_conference::ShaggyFurEffect> shaggy_fur_;
};

TEST_F(BubbleViewTest, NoEffects) {
  EXPECT_FALSE(bubble_view());

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(bubble_view());
  EXPECT_TRUE(bubble_view()->GetVisible());

  // "Return to app" is present and visible.
  EXPECT_TRUE(return_to_app());
  EXPECT_TRUE(return_to_app()->GetVisible());

  // No effects added, no effects view(s) present.
  EXPECT_FALSE(toggle_effects_view());

  // Click the toggle button, bubble is taken down.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(bubble_view());
}

TEST_F(BubbleViewTest, RegisterToggleEffect) {
  // Open up the bubble, no toggle effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Open up the bubble, toggle effects container view is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effects_view());
  EXPECT_TRUE(toggle_effects_view()->GetVisible());
}

TEST_F(BubbleViewTest, UnregisterToggleEffect) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Open up the bubble, toggle effects are present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effects_view());
  EXPECT_TRUE(toggle_effects_view()->GetVisible());

  // Take down the bubble.
  LeftClickOn(toggle_bubble_button());

  // Remove the toggle effect.
  controller()->effects_manager().UnregisterDelegate(office_bunny());

  // Open up the bubble again, no effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());
}

TEST_F(BubbleViewTest, ToggleButtonClicked) {
  // Add one toggle effect.
  controller()->effects_manager().RegisterDelegate(office_bunny());

  // Click to open the bubble, toggle effect button is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(toggle_effect_button());
  EXPECT_TRUE(toggle_effect_button()->GetVisible());

  // Toggle effect button has not yet been clicked.
  EXPECT_EQ(office_bunny()->num_activations_for_testing(), 0);

  // Click the toggle effect button, verify that the effect has been "activated"
  // once.
  LeftClickOn(toggle_effect_button());
  EXPECT_EQ(office_bunny()->num_activations_for_testing(), 1);
}

TEST_F(BubbleViewTest, RegisterSetValueEffect) {
  // Open up the bubble, no set-value effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(toggle_effects_view());

  // Close the bubble.
  LeftClickOn(toggle_bubble_button());

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Open up the bubble, set-value effects container view is present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(set_value_effects_view());
  EXPECT_TRUE(set_value_effects_view()->GetVisible());
}

TEST_F(BubbleViewTest, UnregisterSetValueEffect) {
  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Open up the bubble, set-value effects are present/visible.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(set_value_effects_view());
  EXPECT_TRUE(set_value_effects_view()->GetVisible());

  // Take down the bubble.
  LeftClickOn(toggle_bubble_button());

  // Remove the set-value effect.
  controller()->effects_manager().UnregisterDelegate(shaggy_fur());

  // Open up the bubble again, no effects present.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(set_value_effects_view());
}

TEST_F(BubbleViewTest, SetValueButtonClicked) {
  // Verify that the delegate hosts a single effect which has at least two
  // values.
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 1);
  EXPECT_GE(shaggy_fur()->GetEffect(0)->GetNumStates(), 2);

  // Add one set-value effect.
  controller()->effects_manager().RegisterDelegate(shaggy_fur());

  // Click to open the bubble, effect value 0 button is present/visible.
  LeftClickOn(toggle_bubble_button());
  views::View* button = GetSetValueEffectButton(0);
  EXPECT_TRUE(button);
  EXPECT_TRUE(button->GetVisible());

  // Effect button for value 0 has not yet been clicked.
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 0);

  // Click the effect value 0 button, verify that the value has been "activated"
  // once.
  LeftClickOn(button);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 1);

  // Now test another button, confirm that set-value effect button 1 is
  // present/visible.
  button = GetSetValueEffectButton(1);
  EXPECT_TRUE(button);
  EXPECT_TRUE(button->GetVisible());

  // Effect button for value 1 has not yet been clicked.
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(1), 0);

  // Click the effect value 1 button, verify that value 1 has been "activated"
  // once, and confirm that value 0 has still only been activated once i.e. we
  // just activated value 1 and not value 0.
  LeftClickOn(button);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(1), 1);
  EXPECT_EQ(shaggy_fur()->GetNumActivationsForTesting(0), 1);
}
}  // namespace ash::video_conference