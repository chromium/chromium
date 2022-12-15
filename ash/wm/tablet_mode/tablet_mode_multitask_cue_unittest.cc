// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/wm/features.h"

namespace ash {

namespace {

constexpr int kCueYOffset = 6;
constexpr int kCueWidth = 48;
constexpr int kCueHeight = 4;

}  // namespace

class TabletModeMultitaskCueTest : public AshTestBase {
 public:
  TabletModeMultitaskCueTest()
      : scoped_feature_list_(chromeos::wm::features::kFloatWindow) {}
  TabletModeMultitaskCueTest(const TabletModeMultitaskCueTest&) = delete;
  TabletModeMultitaskCueTest& operator=(const TabletModeMultitaskCueTest&) =
      delete;
  ~TabletModeMultitaskCueTest() override = default;

  TabletModeMultitaskCue* GetMultitaskCue() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_cue_for_testing();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabletModeMultitaskCueTest, BasicShowCue) {
  auto window = CreateAppWindow();

  auto* multitask_cue = GetMultitaskCue();
  ASSERT_TRUE(multitask_cue);

  ui::Layer* cue_layer = multitask_cue->cue_layer_for_testing();
  ASSERT_TRUE(cue_layer);
  EXPECT_EQ(
      gfx::Rect((800 - kCueWidth) / 2, kCueYOffset, kCueWidth, kCueHeight),
      cue_layer->bounds());
}

}  // namespace ash