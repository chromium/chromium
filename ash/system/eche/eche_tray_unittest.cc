// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <memory>
#include <string>

#include "ash/shell.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

class EcheTrayTest : public AshTestBase {
 public:
  EcheTrayTest() = default;

  EcheTrayTest(const EcheTrayTest&) = delete;
  EcheTrayTest& operator=(const EcheTrayTest&) = delete;

  ~EcheTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kEcheSWA,
                              chromeos::features::kEcheCustomWidget,
                              chromeos::features::kEcheSWAInBackground},
        /*disabled_features=*/{});

    DCHECK(test_web_view_factory_.get());

    AshTestBase::SetUp();

    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();

    display::test::DisplayManagerTestApi(display_manager())
        .SetFirstDisplayAsInternalDisplay();
  }

  // Performs a tap on the eche tray button.
  void PerformTap() {
    GetEventGenerator()->GestureTapAt(
        eche_tray_->GetBoundsInScreen().CenterPoint());
  }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  EcheTray* eche_tray_ = nullptr;  // Not owned
  base::test::ScopedFeatureList feature_list_;

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

// Verify the Eche tray button exists and but is not visible initially.
TEST_F(EcheTrayTest, PaletteTrayIsInvisible) {
  ASSERT_TRUE(eche_tray());
  EXPECT_FALSE(eche_tray()->GetVisible());
}

// Verify taps on the eche tray button results in expected behaviour.
// It also sets the url and calls `ShowBubble`.
TEST_F(EcheTrayTest, EcheTrayShowBubbleAndTapTwice) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(eche_tray()->GetVisible());

  eche_tray()->SetUrl(GURL("http://google.com"));
  eche_tray()->SetVisiblePreferred(true);
  eche_tray()->ShowBubble();

  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  // Verify that by tapping the eche tray button, the button will become
  // inactive and the eche tray bubble will be closed.
  PerformTap();
  // Wait for the tray bubble widget to close.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()->GetVisible());

  // Verify that tapping again will show the bubble.
  PerformTap();
  // Wait for the tray bubble widget to open.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()->GetVisible());
}

TEST_F(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst) {
  // Verify the eche tray button is not active, and the eche tray bubble
  // is not shown initially.
  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());

  // Allow us to create the bubble but it is not visible until we need this
  // bubble to show up.
  eche_tray()->SetUrl(GURL("http://google.com"));
  eche_tray()->InitBubble();
  eche_tray()->HideBubble();

  EXPECT_FALSE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_TRUE(eche_tray()->loading_indicator_->GetAnimating());

  // Request this bubble to show up.
  eche_tray()->ShowBubble();
  // Wait for the tray bubble widget to open.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(eche_tray()->loading_indicator_->GetAnimating());
}

}  // namespace ash
