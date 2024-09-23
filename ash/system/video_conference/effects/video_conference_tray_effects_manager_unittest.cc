// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/audio_effects_controller.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// A test effect delegate for registering a fake effect with id
// `VcEffectId::kLiveCaption`.
class TestEffectDelegate : public VcEffectsDelegate {
 public:
  TestEffectDelegate() {
    std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
        /*type=*/VcEffectType::kToggle,
        /*get_state_callback=*/base::BindRepeating([]() {
          return std::optional(0);
        }),
        /*effect_id=*/VcEffectId::kLiveCaption);
    auto effect_state = std::make_unique<VcEffectState>(
        /*icon=*/&kVideoConferenceLiveCaptionOnIcon,
        /*label_text=*/u"Dummy label",
        /*accessible_name_id=*/IDS_ASH_STATUS_TRAY_LIVE_CAPTION,
        /*button_callback=*/base::RepeatingClosure());
    effect->AddState(std::move(effect_state));
    AddEffect(std::move(effect));
  }
  TestEffectDelegate(const TestEffectDelegate&) = delete;
  TestEffectDelegate& operator=(const TestEffectDelegate&) = delete;
  ~TestEffectDelegate() override = default;

  // VcEffectsDelegate:
  std::optional<int> GetEffectState(VcEffectId effect_id) override {
    return std::nullopt;
  }
  void OnEffectControlActivated(VcEffectId effect_id,
                                std::optional<int> state) override {}
};

class VideoConferenceTrayEffectsManagerTest
    : public AshTestBase,
      public testing::WithParamInterface</*IsVcDlcUiEnabled*/ bool> {
 public:
  VideoConferenceTrayEffectsManagerTest() = default;
  VideoConferenceTrayEffectsManagerTest(
      const VideoConferenceTrayEffectsManagerTest&) = delete;
  VideoConferenceTrayEffectsManagerTest& operator=(
      const VideoConferenceTrayEffectsManagerTest&) = delete;
  ~VideoConferenceTrayEffectsManagerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {};
    if (IsVcDlcUiEnabled()) {
      enabled_features.push_back(features::kFeatureManagementVideoConference);
      enabled_features.push_back(features::kVcDlcUi);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_features=*/{});
    effects_manager_ = std::make_unique<VideoConferenceTrayEffectsManager>();
    if (IsVcDlcUiEnabled()) {
      // Need to create a fake VC tray controller if VcDlcUi is enabled because
      // this implies `features::IsVideoConferenceEnabled()` is true, and when
      // that is true the VC tray is created (and the VC tray depends on the
      // VC tray controller being available).
      tray_controller_ = std::make_unique<FakeVideoConferenceTrayController>();
      effect_delegate_ = std::make_unique<TestEffectDelegate>();
      DlcserviceClient::InitializeFake();
    }
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    if (IsVcDlcUiEnabled()) {
      DlcserviceClient::Shutdown();
      effect_delegate_.reset();
      tray_controller_.reset();
    }
    effects_manager_.reset();
  }

  bool IsVcDlcUiEnabled() { return GetParam(); }

  VideoConferenceTrayEffectsManager* effects_manager() {
    return effects_manager_.get();
  }

  TestEffectDelegate* effect_delegate() { return effect_delegate_.get(); }

 private:
  std::unique_ptr<VideoConferenceTrayEffectsManager> effects_manager_;
  std::unique_ptr<FakeVideoConferenceTrayController> tray_controller_;
  std::unique_ptr<TestEffectDelegate> effect_delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IsVcDlcUiEnabled,
                         VideoConferenceTrayEffectsManagerTest,
                         testing::Bool());

TEST_P(VideoConferenceTrayEffectsManagerTest, GetToggleEffectButtonTable) {
  VideoConferenceTrayEffectsManager::EffectDataTable buttons_table;

  // No effects registered, no buttons, empty table.
  EXPECT_FALSE(effects_manager()->HasToggleEffects());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_TRUE(buttons_table.empty());

  // Register an effect.
  std::unique_ptr<fake_video_conference::CatEarsEffect> cat_ears =
      std::make_unique<fake_video_conference::CatEarsEffect>();
  effects_manager()->RegisterDelegate(cat_ears.get());
  EXPECT_TRUE(effects_manager()->HasToggleEffects());

  // Table should have one row with one button.
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 1UL);
  EXPECT_EQ(buttons_table[0].size(), 1UL);

  // Unregister the effect.
  effects_manager()->UnregisterDelegate(cat_ears.get());
  EXPECT_FALSE(effects_manager()->HasToggleEffects());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_TRUE(buttons_table.empty());

  // Add the first effect back, and one more. Verify that the table still has
  // one row, but now two buttons.
  effects_manager()->RegisterDelegate(cat_ears.get());
  std::unique_ptr<fake_video_conference::DogFurEffect> dog_fur =
      std::make_unique<fake_video_conference::DogFurEffect>();
  effects_manager()->RegisterDelegate(dog_fur.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 1UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);

  // One more effect, still one row but three buttons.
  std::unique_ptr<fake_video_conference::SpaceshipEffect> spaceship =
      std::make_unique<fake_video_conference::SpaceshipEffect>();
  effects_manager()->RegisterDelegate(spaceship.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 1UL);
  EXPECT_EQ(buttons_table[0].size(), 3UL);

  // Four effects, now two rows each with two buttons.
  std::unique_ptr<fake_video_conference::OfficeBunnyEffect> office_bunny =
      std::make_unique<fake_video_conference::OfficeBunnyEffect>();
  effects_manager()->RegisterDelegate(office_bunny.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 2UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);
  EXPECT_EQ(buttons_table[1].size(), 2UL);

  // Five effects, now three rows of 2-2-1.
  std::unique_ptr<fake_video_conference::CalmForestEffect> calm_forest =
      std::make_unique<fake_video_conference::CalmForestEffect>();
  effects_manager()->RegisterDelegate(calm_forest.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 3UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);
  EXPECT_EQ(buttons_table[1].size(), 2UL);
  EXPECT_EQ(buttons_table[2].size(), 1UL);

  // Six effects, now three rows of 2-2-2.
  std::unique_ptr<fake_video_conference::StylishKitchenEffect> stylish_kitchen =
      std::make_unique<fake_video_conference::StylishKitchenEffect>();
  effects_manager()->RegisterDelegate(stylish_kitchen.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 3UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);
  EXPECT_EQ(buttons_table[1].size(), 2UL);
  EXPECT_EQ(buttons_table[2].size(), 2UL);

  // Seven effects, now four rows of 2-2-2-1.
  auto long_text_label_effect =
      std::make_unique<fake_video_conference::FakeLongTextLabelToggleEffect>();
  effects_manager()->RegisterDelegate(long_text_label_effect.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 4UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);
  EXPECT_EQ(buttons_table[1].size(), 2UL);
  EXPECT_EQ(buttons_table[2].size(), 2UL);
  EXPECT_EQ(buttons_table[3].size(), 1UL);

  // Now add a set-value effect, which should not affect the amount or
  // arrangement of toggle effects.
  std::unique_ptr<fake_video_conference::ShaggyFurEffect> shaggy_fur =
      std::make_unique<fake_video_conference::ShaggyFurEffect>();
  effects_manager()->RegisterDelegate(shaggy_fur.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_EQ(buttons_table.size(), 4UL);
  EXPECT_EQ(buttons_table[0].size(), 2UL);
  EXPECT_EQ(buttons_table[1].size(), 2UL);
  EXPECT_EQ(buttons_table[2].size(), 2UL);
  EXPECT_EQ(buttons_table[3].size(), 1UL);

  // Cleanup.
  effects_manager()->UnregisterDelegate(cat_ears.get());
  effects_manager()->UnregisterDelegate(dog_fur.get());
  effects_manager()->UnregisterDelegate(spaceship.get());
  effects_manager()->UnregisterDelegate(office_bunny.get());
  effects_manager()->UnregisterDelegate(calm_forest.get());
  effects_manager()->UnregisterDelegate(stylish_kitchen.get());
  effects_manager()->UnregisterDelegate(long_text_label_effect.get());
  effects_manager()->UnregisterDelegate(shaggy_fur.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_TRUE(buttons_table.empty());
}

// Tests that a tile UI controller is not created for an unsupported toggle
// effect.
TEST_P(VideoConferenceTrayEffectsManagerTest,
       NoTileUiControllerForUnsupportedToggleEffect) {
  // Tile UI controllers are only used when `VcDlcUi` is enabled.
  if (!IsVcDlcUiEnabled()) {
    return;
  }

  // Verify that there are no supported toggle effects.
  EXPECT_EQ(0u, effects_manager()->GetToggleEffects().size());

  // Verify that no tile UI controller is returned when one is requested.
  EXPECT_FALSE(
      effects_manager()->GetUiControllerForEffectId(VcEffectId::kLiveCaption));
}

// Tests that a tile UI controller is added the first time it is requested.
TEST_P(VideoConferenceTrayEffectsManagerTest,
       TileUiControllerAddedWhenFirstRequested) {
  // Tile UI controllers are only used when `VcDlcUi` is enabled.
  if (!IsVcDlcUiEnabled()) {
    return;
  }

  // Register the test "Live caption" effect.
  effects_manager()->RegisterDelegate(effect_delegate());

  // Verify that a tile UI controller is returned when one is requested for the
  // test "Live caption" effect.
  EXPECT_TRUE(
      effects_manager()->GetUiControllerForEffectId(VcEffectId::kLiveCaption));

  // Cleanup.
  effects_manager()->UnregisterDelegate(effect_delegate());
}

// Tests that unregistering effects removes the tile UI controllers for those
// effects.
TEST_P(VideoConferenceTrayEffectsManagerTest,
       RemovesTileUiControllersForUnregisteredEffects) {
  // Tile UI controllers are only used when `VcDlcUi` is enabled.
  if (!IsVcDlcUiEnabled()) {
    return;
  }

  // Register the test "Live caption" effect.
  effects_manager()->RegisterDelegate(effect_delegate());

  // Verify that there is a tile UI controller for the test "Live caption"
  // effect.
  EXPECT_TRUE(
      effects_manager()->GetUiControllerForEffectId(VcEffectId::kLiveCaption));

  // Un-register the test "Live caption" effect.
  effects_manager()->UnregisterDelegate(effect_delegate());

  // Verify that there is no longer a tile UI controller for the test
  // "Live caption" effect.
  EXPECT_FALSE(
      effects_manager()->GetUiControllerForEffectId(VcEffectId::kLiveCaption));
}

}  // namespace ash
