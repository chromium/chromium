// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"

#include <memory>

#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"

namespace ash::video_conference {

namespace {

class TestHostedEffect : public VcHostedEffect {
 public:
  explicit TestHostedEffect(VcEffectId effect_id)
      : VcHostedEffect(VcEffectType::kToggle,
                       /*get_state_callback=*/
                       base::BindRepeating(&TestHostedEffect::GetEffectState,
                                           base::Unretained(this)),
                       effect_id) {}
  TestHostedEffect(const TestHostedEffect&) = delete;
  TestHostedEffect& operator=(const TestHostedEffect&) = delete;
  ~TestHostedEffect() = default;

  std::optional<int> GetEffectState() { return 0; }
};

}  // namespace

class VideoConferenceTrayEffectsDelegateTest : public AshTestBase {
 public:
  VideoConferenceTrayEffectsDelegateTest() = default;
  VideoConferenceTrayEffectsDelegateTest(
      const VideoConferenceTrayEffectsDelegateTest&) = delete;
  VideoConferenceTrayEffectsDelegateTest& operator=(
      const VideoConferenceTrayEffectsDelegateTest&) = delete;
  ~VideoConferenceTrayEffectsDelegateTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Instantiate these fake effects, to be registered/unregistered as needed.
    shaggy_fur_ = std::make_unique<fake_video_conference::ShaggyFurEffect>();

    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    shaggy_fur_.reset();
  }
  fake_video_conference::ShaggyFurEffect* shaggy_fur() {
    return shaggy_fur_.get();
  }

 private:
  std::unique_ptr<fake_video_conference::ShaggyFurEffect> shaggy_fur_;
};

TEST_F(VideoConferenceTrayEffectsDelegateTest, AddAndRemoveEffect) {
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 1);

  shaggy_fur()->AddEffect(
      std::make_unique<TestHostedEffect>(VcEffectId::kBackgroundBlur));
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 2);

  shaggy_fur()->AddEffect(
      std::make_unique<TestHostedEffect>(VcEffectId::kPortraitRelighting));
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 3);

  // Adding new effect with existing id should just replace the old one.
  shaggy_fur()->AddEffect(
      std::make_unique<TestHostedEffect>(VcEffectId::kBackgroundBlur));
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 3);

  // Tests removing effect.
  EXPECT_TRUE(shaggy_fur()->GetEffectById(VcEffectId::kPortraitRelighting));

  shaggy_fur()->RemoveEffect(VcEffectId::kPortraitRelighting);

  EXPECT_FALSE(shaggy_fur()->GetEffectById(VcEffectId::kPortraitRelighting));
  EXPECT_EQ(shaggy_fur()->GetNumEffects(), 2);
}

}  // namespace ash::video_conference
