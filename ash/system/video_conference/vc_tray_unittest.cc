// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/vc_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class VcTrayTest : public AshTestBase {
 public:
  VcTrayTest() = default;
  VcTrayTest(const VcTrayTest&) = delete;
  VcTrayTest& operator=(const VcTrayTest&) = delete;
  ~VcTrayTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVcControlsUi);

    AshTestBase::SetUp();
  }

  VcTray* vc_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->vc_tray();
  }

  // Generate a click for a button.
  void ClickButton(views::Button* button) {
    GetEventGenerator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(VcTrayTest, ClickTrayButton) {
  EXPECT_FALSE(vc_tray()->GetBubbleView());

  // Clicking the tray button should construct and open up the bubble.
  ClickButton(vc_tray());
  EXPECT_TRUE(vc_tray()->GetBubbleView());
  EXPECT_TRUE(vc_tray()->GetBubbleView()->GetVisible());

  // Clicking it again should reset the bubble.
  ClickButton(vc_tray());
  EXPECT_FALSE(vc_tray()->GetBubbleView());

  ClickButton(vc_tray());
  EXPECT_TRUE(vc_tray()->GetBubbleView());
  EXPECT_TRUE(vc_tray()->GetBubbleView()->GetVisible());

  // Click anywhere else outside the bubble (i.e. the status area button) should
  // close the bubble.
  ClickButton(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray());
  EXPECT_FALSE(vc_tray()->GetBubbleView());
}

}  // namespace ash