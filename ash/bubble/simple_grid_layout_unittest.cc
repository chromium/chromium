// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/simple_grid_layout.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

class SimpleGridLayoutTest : public testing::Test {
 public:
  void SetUp() override { host_ = std::make_unique<views::View>(); }

  std::unique_ptr<views::View> host_;
};

TEST_F(SimpleGridLayoutTest, ChildIgnoredByLayout) {
  // Create a SimpleGrid host view for 2 columns.
  views::LayoutManagerBase* layout =
      host_->SetLayoutManager(std::make_unique<SimpleGridLayout>(2, 0, 0));

  // Add a child view of size 20 and verify that the Grid has a correct size.
  views::StaticSizedView* v1 = host_->AddChildView(
      std::make_unique<views::StaticSizedView>(gfx::Size(20, 20)));
  EXPECT_EQ(gfx::Size(40, 20), layout->GetPreferredSize(host_.get()));

  // Add a child view of size 10 and verify that the Grid assumes all views are
  // size 20. as the first child.
  views::StaticSizedView* v2 = host_->AddChildView(
      std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)));
  EXPECT_EQ(gfx::Size(40, 20), layout->GetPreferredSize(host_.get()));

  // Add a third child view of size 10 and verify that the grid is sozed
  // correctly for 2 rows.
  views::StaticSizedView* v3 = host_->AddChildView(
      std::make_unique<views::StaticSizedView>(gfx::Size(10, 10)));
  EXPECT_EQ(gfx::Size(40, 40), layout->GetPreferredSize(host_.get()));

  // Set the first view to be ignored by layout. Expect the size to change
  // accordingly to consider the child views sized at 10.
  v1->SetProperty(views::kViewIgnoredByLayoutKey, true);
  EXPECT_EQ(gfx::Size(20, 10), layout->GetPreferredSize(host_.get()));

  // Layout the views. Verify the coordinates for all views.
  views::test::RunScheduledLayout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v2->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), v3->bounds());

  // Change the host size view and run a layout again. Verify the layout did not
  // changed.
  host_->SetBounds(0, 0, 30, 30);
  views::test::RunScheduledLayout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v2->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), v3->bounds());
}

}  // namespace ash
