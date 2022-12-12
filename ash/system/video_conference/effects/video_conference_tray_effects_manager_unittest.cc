// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class VideoConferenceTrayEffectsManagerTest : public AshTestBase {
 public:
  VideoConferenceTrayEffectsManagerTest() = default;
  VideoConferenceTrayEffectsManagerTest(
      const VideoConferenceTrayEffectsManagerTest&) = delete;
  VideoConferenceTrayEffectsManagerTest& operator=(
      const VideoConferenceTrayEffectsManagerTest&) = delete;
  ~VideoConferenceTrayEffectsManagerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    effects_manager_ = std::make_unique<VideoConferenceTrayEffectsManager>();
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    effects_manager_.reset();
  }

  VideoConferenceTrayEffectsManager* effects_manager() {
    return effects_manager_.get();
  }

 private:
  std::unique_ptr<VideoConferenceTrayEffectsManager> effects_manager_;
};

TEST_F(VideoConferenceTrayEffectsManagerTest, GetToggleEffectButtonTable) {
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
  std::unique_ptr<fake_video_conference::GreenhouseEffect> greenhouse =
      std::make_unique<fake_video_conference::GreenhouseEffect>();
  effects_manager()->RegisterDelegate(greenhouse.get());
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
  effects_manager()->UnregisterDelegate(greenhouse.get());
  effects_manager()->UnregisterDelegate(shaggy_fur.get());
  buttons_table = effects_manager()->GetToggleEffectButtonTable();
  EXPECT_TRUE(buttons_table.empty());
}

}  // namespace ash