// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bloom/bloom_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

using chromeos::bloom::BloomController;
using chromeos::bloom::BloomInteractionResolution;

// Fake |BloomController| that will be stored as the singleton
// |BloomController|, and that simply tracks if there is any interaction.
class ScopedBloomController : public BloomController {
 public:
  // BloomController implementation:
  void StartInteraction() override {
    EXPECT_FALSE(has_interaction_);
    has_interaction_ = true;
  }

  bool HasInteraction() const override { return has_interaction_; }

  void StopInteraction(BloomInteractionResolution resolution) override {
    EXPECT_TRUE(has_interaction_);
    resolution_ = resolution;
    has_interaction_ = false;
  }

  BloomInteractionResolution GetLastInteractionResolution() const {
    return resolution_;
  }

 private:
  bool has_interaction_ = false;
  BloomInteractionResolution resolution_;
};

}  // namespace

class BloomTrayTest : public AshTestBase {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::assistant::features::kEnableBloom);

    AshTestBase::SetUp();

    bloom_tray()->SetVisiblePreferred(true);
  }

  ScopedBloomController* bloom_controller() { return &bloom_controller_; }

  BloomTray* bloom_tray() {
    StatusAreaWidget* status =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget();
    return status->bloom_tray_for_testing();
  }

  void TapOn(BloomTray* view) {
    ASSERT_TRUE(view->GetVisible());
    view->PerformAction(
        ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP)));
  }

 private:
  ScopedBloomController bloom_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BloomTrayTest, ShouldStartBloomInteractionOnTap) {
  BloomTray* tray = bloom_tray();

  TapOn(tray);

  EXPECT_TRUE(tray->is_active());
  EXPECT_TRUE(bloom_controller()->HasInteraction());
}

TEST_F(BloomTrayTest, ShouldStopBloomInteractionOnSecondTap) {
  BloomTray* tray = bloom_tray();

  TapOn(tray);

  TapOn(tray);
  EXPECT_FALSE(tray->is_active());
  EXPECT_FALSE(bloom_controller()->HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kNormal,
            bloom_controller()->GetLastInteractionResolution());
}

}  // namespace ash
