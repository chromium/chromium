// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/display_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::View* GetScrollContent(views::View* detailed_view) {
  return detailed_view->GetViewByID(VIEW_ID_QS_DISPLAY_SCROLL_CONTENT);
}

views::View* GetTileContainer(views::View* detailed_view) {
  return detailed_view->GetViewByID(VIEW_ID_QS_DISPLAY_TILE_CONTAINER);
}

}  // namespace

class DisplayDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    AshTestBase::TearDown();
  }

  DetailedViewDelegate* fake_delegate() { return delegate_.get(); }

  std::unique_ptr<DetailedViewDelegate> delegate_;
};

TEST_F(DisplayDetailedViewTest, ScrollContentChildren) {
  DisplayDetailedView detailed_view(fake_delegate(),
                                    /*tray_controller=*/nullptr);

  // The scroll content has two children, one feature tile container and one
  // `UnifiedBrightnessView`.
  views::View* scroll_content = GetScrollContent(&detailed_view);
  ASSERT_TRUE(scroll_content);
  ASSERT_EQ(scroll_content->children().size(), 2u);

  // The first child of scroll content is the `tile_container`, which has two
  // children (night light and dark mode feature tiles).
  views::View* tile_container = GetTileContainer(&detailed_view);
  ASSERT_TRUE(tile_container);
  ASSERT_EQ(tile_container->children().size(), 2u);
  EXPECT_STREQ(tile_container->children()[0]->GetClassName(), "FeatureTile");
  EXPECT_STREQ(tile_container->children()[1]->GetClassName(), "FeatureTile");

  // The second children of scroll content is the `UnifiedBrightnessView`.
  views::View* unified_brightness_view =
      scroll_content->GetViewByID(VIEW_ID_QS_DISPLAY_BRIGHTNESS_SLIDER);
  EXPECT_STREQ(unified_brightness_view->GetClassName(),
               "UnifiedBrightnessView");
}

TEST_F(DisplayDetailedViewTest, FeatureTileVisibility) {
  // Both tiles are visible in the active user session
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  DisplayDetailedView detailed_view1(fake_delegate(),
                                     /*tray_controller=*/nullptr);
  const auto* const tile_container1 = GetTileContainer(&detailed_view1);
  ASSERT_TRUE(tile_container1);
  ASSERT_EQ(tile_container1->children().size(), 2u);
  EXPECT_TRUE(tile_container1->GetVisible());
  EXPECT_TRUE(tile_container1->children()[0]->GetVisible());
  EXPECT_TRUE(tile_container1->children()[1]->GetVisible());

  // Feature tiles are still visible in the locked screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  DisplayDetailedView detailed_view2(fake_delegate(),
                                     /*tray_controller=*/nullptr);
  const auto* const tile_container2 = GetTileContainer(&detailed_view2);
  EXPECT_TRUE(tile_container2->GetVisible());
  EXPECT_TRUE(tile_container2->children()[0]->GetVisible());
  EXPECT_TRUE(tile_container2->children()[1]->GetVisible());

  // Feature tiles are not visible in OOBE.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  DisplayDetailedView detailed_view3(fake_delegate(),
                                     /*tray_controller=*/nullptr);
  const auto* const tile_container3 = GetTileContainer(&detailed_view3);
  EXPECT_FALSE(tile_container3->GetVisible());
  EXPECT_FALSE(tile_container3->children()[0]->GetVisible());
  EXPECT_FALSE(tile_container3->children()[1]->GetVisible());
}

}  // namespace ash
