// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/tray/size_range_layout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_layout_manager.h"
#include "ui/views/view.h"

namespace ash {

class SizeRangeLayoutTest : public testing::Test {
 public:
  SizeRangeLayoutTest();

  SizeRangeLayoutTest(const SizeRangeLayoutTest&) = delete;
  SizeRangeLayoutTest& operator=(const SizeRangeLayoutTest&) = delete;

  // Wrapper function to access the minimum preferred size of |layout|.
  gfx::Size GetMinSize(const SizeRangeLayout* layout) const;

  // Wrapper function to access the maximum preferred size of |layout|.
  gfx::Size GetMaxSize(const SizeRangeLayout* layout) const;

 protected:
  const gfx::Size kAbsoluteMinSize;
  const gfx::Size kAbsoluteMaxSize;
};

SizeRangeLayoutTest::SizeRangeLayoutTest()
    : kAbsoluteMinSize(SizeRangeLayout::kAbsoluteMinSize,
                       SizeRangeLayout::kAbsoluteMinSize),
      kAbsoluteMaxSize(SizeRangeLayout::kAbsoluteMaxSize,
                       SizeRangeLayout::kAbsoluteMaxSize) {}

gfx::Size SizeRangeLayoutTest::GetMinSize(const SizeRangeLayout* layout) const {
  return layout->min_size_;
}

gfx::Size SizeRangeLayoutTest::GetMaxSize(const SizeRangeLayout* layout) const {
  return layout->max_size_;
}

TEST_F(SizeRangeLayoutTest, SizeRangeForDefaultConstruction) {
  SizeRangeLayout layout;
  EXPECT_EQ(kAbsoluteMinSize, GetMinSize(&layout));
  EXPECT_EQ(kAbsoluteMaxSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest, SizeRangeForExplicitConstruction) {
  const gfx::Size kSmallSize = gfx::Size(13, 14);
  const gfx::Size kLargeSize = gfx::Size(25, 26);

  SizeRangeLayout layout(kSmallSize, kLargeSize);
  EXPECT_EQ(kSmallSize, GetMinSize(&layout));
  EXPECT_EQ(kLargeSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest, InvalidMinSizeForExplicitConstruction) {
  const gfx::Size kInvalidSmallSize(-1, 2);
  const gfx::Size kExpectedMinSize(0, 2);

  SizeRangeLayout layout(kInvalidSmallSize, kAbsoluteMaxSize);
  EXPECT_EQ(kExpectedMinSize, GetMinSize(&layout));
}

TEST_F(SizeRangeLayoutTest, InvalidMaxSizeForExplicitConstruction) {
  const gfx::Size kInvalidSmallSize(-1, 2);
  const gfx::Size kExpectedMinSize(0, 2);

  SizeRangeLayout layout(kInvalidSmallSize, kAbsoluteMaxSize);
  EXPECT_EQ(kExpectedMinSize, GetMinSize(&layout));
}

TEST_F(SizeRangeLayoutTest, MaxSizeSmallerThanMinSizeConstruction) {
  const gfx::Size kMinSize(10, 11);
  const gfx::Size kMaxSize(5, 6);

  SizeRangeLayout layout(kMinSize, kMaxSize);
  EXPECT_EQ(kMaxSize, GetMinSize(&layout));
  EXPECT_EQ(kMaxSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest, SizeRangeForExplicitSetSize) {
  const gfx::Size kSize = gfx::Size(13, 14);

  SizeRangeLayout layout;
  EXPECT_NE(kSize, GetMinSize(&layout));
  EXPECT_NE(kSize, GetMaxSize(&layout));

  layout.SetSize(kSize);
  EXPECT_EQ(kSize, GetMinSize(&layout));
  EXPECT_EQ(kSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest, InvalidSizeRangesForExplicitSetSize) {
  const gfx::Size kInvalidSize(-7, 8);
  const gfx::Size kExpectedSize(0, 8);

  SizeRangeLayout layout;
  layout.SetSize(kInvalidSize);
  EXPECT_EQ(kExpectedSize, GetMinSize(&layout));
  EXPECT_EQ(kExpectedSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest, InternalLayoutManagerPreferredSizeIsUsed) {
  const gfx::Size kSize(7, 8);
  std::unique_ptr<views::test::TestLayoutManager> child_layout =
      std::make_unique<views::test::TestLayoutManager>();
  child_layout->SetPreferredSize(kSize);

  SizeRangeLayout layout;
  EXPECT_NE(kSize, layout.GetPreferredSize());

  layout.SetLayoutManager(std::move(child_layout));
  EXPECT_EQ(kSize, layout.GetPreferredSize());
}

TEST_F(SizeRangeLayoutTest, SmallPreferredSizeIsClamped) {
  const gfx::Size kMinSize(10, 10);
  const gfx::Size kMaxSize(20, 20);
  const gfx::Size kLayoutPreferredSize(5, 5);
  std::unique_ptr<views::test::TestLayoutManager> child_layout =
      std::make_unique<views::test::TestLayoutManager>();
  child_layout->SetPreferredSize(kLayoutPreferredSize);

  SizeRangeLayout layout;
  layout.SetLayoutManager(std::move(child_layout));
  layout.SetMinSize(kMinSize);
  layout.SetMaxSize(kMaxSize);
  EXPECT_EQ(kMinSize, layout.GetPreferredSize());
}

TEST_F(SizeRangeLayoutTest, LargePreferredSizeIsClamped) {
  const gfx::Size kMinSize(10, 10);
  const gfx::Size kMaxSize(20, 20);
  const gfx::Size kLayoutPreferredSize(25, 25);
  std::unique_ptr<views::test::TestLayoutManager> child_layout =
      std::make_unique<views::test::TestLayoutManager>();
  child_layout->SetPreferredSize(kLayoutPreferredSize);

  SizeRangeLayout layout;
  layout.SetLayoutManager(std::move(child_layout));
  layout.SetMinSize(kMinSize);
  layout.SetMaxSize(kMaxSize);
  EXPECT_EQ(kMaxSize, layout.GetPreferredSize());
}

TEST_F(SizeRangeLayoutTest, MaxSizeLargerThanMinSizeUpdatesMinSize) {
  const gfx::Size kMinSize(10, 10);
  const gfx::Size kMaxSize(5, 5);

  SizeRangeLayout layout;
  layout.SetMinSize(kMinSize);
  EXPECT_EQ(kMinSize, GetMinSize(&layout));
  layout.SetMaxSize(kMaxSize);
  EXPECT_EQ(kMaxSize, GetMinSize(&layout));
}

TEST_F(SizeRangeLayoutTest, MinSizeSmallerThanMaxSizeUpdatesMaxSize) {
  const gfx::Size kMinSize(10, 10);
  const gfx::Size kMaxSize(5, 5);

  SizeRangeLayout layout;
  layout.SetMaxSize(kMaxSize);
  EXPECT_EQ(kMaxSize, GetMaxSize(&layout));
  layout.SetMinSize(kMinSize);
  EXPECT_EQ(kMinSize, GetMaxSize(&layout));
}

TEST_F(SizeRangeLayoutTest,
       InternalLayoutManagerPreferredHeightForWidthIsUsed) {
  const int kWidth = 5;
  const int kHeight = 9;
  std::unique_ptr<views::test::TestLayoutManager> child_layout =
      std::make_unique<views::test::TestLayoutManager>();
  child_layout->set_preferred_height_for_width(kHeight);

  SizeRangeLayout layout;
  EXPECT_NE(kHeight, layout.GetHeightForWidth(kWidth));

  layout.SetLayoutManager(std::move(child_layout));
  EXPECT_EQ(kHeight, layout.GetHeightForWidth(kWidth));
}

}  // namespace ash
