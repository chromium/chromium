// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"

#include <memory>
#include <optional>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

class VcTileUiControllerTest : public AshTestBase {
 public:
  VcTileUiControllerTest() = default;
  VcTileUiControllerTest(const VcTileUiControllerTest&) = delete;
  VcTileUiControllerTest& operator=(const VcTileUiControllerTest&) = delete;
  ~VcTileUiControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create a test VC toggle effect.
    toggle_effect_ = std::make_unique<VcHostedEffect>(
        /*type=*/VcEffectType::kToggle,
        /*get_state_callback=*/
        base::BindRepeating(&VcTileUiControllerTest::GetEffectState,
                            base::Unretained(this)),
        /*effect_id=*/VcEffectId::kTestEffect);
    toggle_effect_->AddState(std::make_unique<VcEffectState>(
        /*icon=*/&kVideoConferenceNoiseCancellationOnIcon,
        /*label_text=*/u"Dummy label",
        /*accessible_name_id=*/
        IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION,
        /*button_callback=*/
        base::BindRepeating(&VcTileUiControllerTest::ButtonCallback,
                            base::Unretained(this))));
    test_controller_ =
        std::make_unique<VcTileUiController>(toggle_effect_.get());
    haptics_tracker_ = std::make_unique<HapticsTrackingTestInputController>();

    // Create a test `views::Widget` and place the test tile in it.
    test_widget_ = CreateFramelessTestWidget();
    test_widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    auto test_tile = test_controller()->CreateTile();
    test_tile_ = test_tile->GetWeakPtr();
    test_widget_->SetContentsView(std::move(test_tile));
  }
  void TearDown() override {
    test_widget_.reset();
    haptics_tracker_.reset();
    test_controller_.reset();
    AshTestBase::TearDown();
  }

  VcEffectId GetTestEffectId() { return test_controller()->effect_id_; }

  // Returns the count of haptics effects since the test started. If `toggle_on`
  // is true then the returned count corresponds to the "toggle on" haptic, and
  // if it is false then the returned count corresponds to the "toggle off"
  // haptic.
  int GetHapticsToggleCount(bool toggle_on) {
    return haptics_tracker_->GetSentHapticCount(
        toggle_on ? ui::HapticTouchpadEffect::kToggleOn
                  : ui::HapticTouchpadEffect::kToggleOff,
        ui::HapticTouchpadEffectStrength::kMedium);
  }

  // Returns the expected tooltip text given the toggle state.
  std::u16string GetToggleButtonExpectedTooltipText(bool toggle_on) {
    return l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION),
        l10n_util::GetStringUTF16(
            toggle_on ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                      : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF));
  }

  void TrackToggleUMA(bool target_toggle_state) {
    test_controller()->TrackToggleUMA(target_toggle_state);
  }

  void PlayToggleHaptic(bool target_toggle_state) {
    test_controller()->PlayToggleHaptic(target_toggle_state);
  }

  void SetInitialToggleState(bool target_toggle_state) {
    initial_toggle_state_ = target_toggle_state;
  }

  void ButtonCallback() { ++button_callback_invocation_count_; }

  size_t GetButtonCallbackInvocationCount() {
    return button_callback_invocation_count_;
  }

  FeatureTile* test_tile() { return test_tile_.get(); }
  VcTileUiController* test_controller() { return test_controller_.get(); }

 private:
  std::optional<int> GetEffectState() { return initial_toggle_state_; }

  std::unique_ptr<VcHostedEffect> toggle_effect_ = nullptr;
  std::unique_ptr<views::Widget> test_widget_ = nullptr;
  std::unique_ptr<VcTileUiController> test_controller_ = nullptr;
  std::unique_ptr<HapticsTrackingTestInputController> haptics_tracker_ =
      nullptr;
  base::WeakPtr<FeatureTile> test_tile_ = nullptr;
  bool initial_toggle_state_ = false;
  size_t button_callback_invocation_count_ = 0;
};

// Tests that the tile's initial toggle state matches the state of the
// corresponding VC effect.
TEST_F(VcTileUiControllerTest, InitialToggleState) {
  // Explicitly set the initial effect state to be off.
  SetInitialToggleState(false);

  // Verify that a tile initialized with this state is toggled-off.
  EXPECT_FALSE(test_controller()->CreateTile()->IsToggled());

  // Explicitly set the initial effect state to be on.
  SetInitialToggleState(true);

  // Verify that a tile initialized with this state is toggled-on.
  EXPECT_TRUE(test_controller()->CreateTile()->IsToggled());
}

// Tests that the tile's initial tooltip matches the state of the corresponding
// VC effect.
TEST_F(VcTileUiControllerTest, InitialTooltip) {
  // Explicitly set the initial effect state to be off.
  SetInitialToggleState(false);

  // Verify that a tile initialized with this state is using a "toggled-off"
  // tooltip.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/false),
            test_controller()->CreateTile()->GetTooltipText());

  // Explicitly set the initial effect state to be on.
  SetInitialToggleState(true);

  // Verify that a tile initialized with this state is using a "toggled-on"
  // tooltip.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/true),
            test_controller()->CreateTile()->GetTooltipText());
}

// Tests that toggling the tile invokes the effect state's button callback.
TEST_F(VcTileUiControllerTest, EffectStateCallbackInvokedWhenToggled) {
  // Verify that the callback has not been called yet.
  ASSERT_EQ(0u, GetButtonCallbackInvocationCount());

  // Toggle the tile.
  LeftClickOn(test_tile());

  // Verify that the callback has been invoked once.
  ASSERT_EQ(1u, GetButtonCallbackInvocationCount());

  // Toggle the tile again.
  LeftClickOn(test_tile());

  // Verify that the callback has been invoked twice.
  ASSERT_EQ(2u, GetButtonCallbackInvocationCount());
}

// Tests that pressing the tile causes it to change its toggle state.
TEST_F(VcTileUiControllerTest, TogglesWhenPressed) {
  // Verify that the test tile is not toggled.
  ASSERT_FALSE(test_tile()->IsToggled());

  // Press the test tile.
  LeftClickOn(test_tile());

  // Verify that the test tile is now toggled.
  EXPECT_TRUE(test_tile()->IsToggled());

  // Press the test tile again.
  LeftClickOn(test_tile());

  // Verify that the test tile is now not toggled.
  EXPECT_FALSE(test_tile()->IsToggled());
}

// Tests that the tooltip is updated when the tile is toggled.
TEST_F(VcTileUiControllerTest, UpdatesTooltipWhenToggled) {
  // Toggle the test tile on.
  LeftClickOn(test_tile());
  ASSERT_TRUE(test_tile()->IsToggled());

  // Verify that the "toggled-on" tooltip is being used.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/true),
            test_tile()->GetTooltipText());

  // Toggle the test tile off.
  LeftClickOn(test_tile());

  // Verify that the "toggled-off" tooltip is now being used.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/false),
            test_tile()->GetTooltipText());
}

// Tests that `VcTileUiController` records toggle metrics when it is instructed
// to.
TEST_F(VcTileUiControllerTest, RecordsHistogramForToggle) {
  // "Toggle-off" test case.
  {
    base::HistogramTester histogram_tester;

    // Instruct the tile controller to track a "toggle-off".
    TrackToggleUMA(/*target_toggle_state=*/false);

    // Verify that the "toggle-off" was recorded.
    histogram_tester.ExpectUniqueSample(
        video_conference_utils::GetEffectHistogramNameForClick(
            GetTestEffectId()),
        /*sample=*/0, /*expected_bucket_count=*/1);
  }

  // "Toggle-on" test case.
  {
    base::HistogramTester histogram_tester;

    // Instruct the tile controller to track a "toggle-on".
    TrackToggleUMA(/*target_toggle_state=*/true);

    // Verify that the "toggle-on" was recorded.
    histogram_tester.ExpectUniqueSample(
        video_conference_utils::GetEffectHistogramNameForClick(
            GetTestEffectId()),
        /*sample=*/1, /*expected_bucket_count=*/1);
  }
}

// Tests that `VcTileUiController` plays haptic toggle effects when it is
// instructed to.
TEST_F(VcTileUiControllerTest, PlaysHapticsForToggle) {
  // Assert there have been no haptics so far.
  ASSERT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/false));
  ASSERT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/true));

  // Instruct the tile controller to play a "toggle-off" haptic.
  PlayToggleHaptic(/*target_toggle_state=*/false);

  // Verify that the "toggle-off" haptic was played.
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/false));
  EXPECT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/true));

  // Instruct the tile controller to play a "toggle-on" haptic.
  PlayToggleHaptic(/*target_toggle_state=*/true);

  // Verify that the "toggle-on" haptic was played.
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/false));
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/true));
}

}  // namespace ash::video_conference
