// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

#include <memory>
#include <set>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "ash/system/holding_space/test_holding_space_item_views_section.h"
#include "ash/system/holding_space/test_holding_space_tray_child_bubble.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {
// HoldingSpaceTrayChildBubbleTestBase -----------------------------------------

class HoldingSpaceTrayChildBubbleTestBase : public HoldingSpaceAshTestBase {
 protected:
  const HoldingSpaceTrayChildBubble* child_bubble() const {
    return child_bubble_;
  }

 private:
  // HoldingSpaceAshTestBase:
  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();

    // Widget.
    // NOTE: The `widget_` is needed so that the `child_bubble_` added to it
    // below will receive prod-like `OnThemeChanged()` events when attached.
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(views::Widget::InitParams{
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET});

    // View delegate.
    view_delegate_ = std::make_unique<HoldingSpaceViewDelegate>(
        /*bubble=*/nullptr);

    // Child bubble.
    child_bubble_ = widget_->GetRootView()->AddChildView(
        CreateChildBubble(view_delegate_.get()));
    child_bubble_->Init();
  }

  void TearDown() override {
    widget_.reset();
    view_delegate_.reset();

    AshTestBase::TearDown();
  }

  // Invoked from `SetUp()` to create the `child_bubble()`.
  virtual std::unique_ptr<HoldingSpaceTrayChildBubble> CreateChildBubble(
      HoldingSpaceViewDelegate* view_delegate) {
    return std::make_unique<TestHoldingSpaceTrayChildBubble>(
        view_delegate, TestHoldingSpaceTrayChildBubble::Params{});
  }

  views::UniqueWidgetPtr widget_;
  std::unique_ptr<HoldingSpaceViewDelegate> view_delegate_;
  raw_ptr<HoldingSpaceTrayChildBubble, DanglingUntriaged> child_bubble_ =
      nullptr;
};

// Tests -----------------------------------------------------------------------

using HoldingSpaceTrayChildBubbleTest = HoldingSpaceTrayChildBubbleTestBase;

TEST_F(HoldingSpaceTrayChildBubbleTest, HasExpectedBubbleTreatment) {
  // Child bubbles should mask child layers to bounds so as not to paint over
  // other child bubbles in the event of overflow.
  auto* layer = child_bubble()->layer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(layer->GetMasksToBounds());

  // Background.
  auto* background = child_bubble()->GetBackground();
  ASSERT_TRUE(background);
  if (chromeos::features::IsJellyEnabled()) {
    EXPECT_EQ(background->get_color(),
              child_bubble()->GetColorProvider()->GetColor(
                  cros_tokens::kCrosSysSystemBaseElevated));
  } else {
    EXPECT_EQ(
        background->get_color(),
        child_bubble()->GetColorProvider()->GetColor(kColorAshShieldAndBase80));
  }
  EXPECT_EQ(layer->background_blur(), ColorProvider::kBackgroundBlurSigma);

  // Border.
  EXPECT_TRUE(child_bubble()->GetBorder());

  // Corner radius.
  EXPECT_TRUE(layer->is_fast_rounded_corner());
  EXPECT_EQ(layer->rounded_corner_radii(),
            gfx::RoundedCornersF(GetBubbleCornerRadius()));
}

// HoldingSpaceTrayChildBubblePlaceholderTest ----------------------------------

class HoldingSpaceTrayChildBubblePlaceholderTest
    : public HoldingSpaceTrayChildBubbleTestBase,
      public testing::WithParamInterface</*has_placeholder=*/bool> {
 protected:
  void ExpectPlaceholderOrGone() {
    if (has_placeholder()) {
      EXPECT_TRUE(child_bubble()->GetVisible());
      EXPECT_EQ(child_bubble()->layer()->opacity(), 1.f);
      EXPECT_FALSE(section_->GetVisible());
      ASSERT_TRUE(placeholder_);
      EXPECT_TRUE(placeholder_->GetVisible());
    } else {
      EXPECT_FALSE(child_bubble()->GetVisible());
      EXPECT_EQ(child_bubble()->layer()->opacity(), 0.f);
      EXPECT_FALSE(section_->GetVisible());
      EXPECT_FALSE(placeholder_);
    }
  }

  void ExpectSection() {
    EXPECT_TRUE(child_bubble()->GetVisible());
    EXPECT_EQ(child_bubble()->layer()->opacity(), 1.f);
    EXPECT_TRUE(section_->GetVisible());
    if (has_placeholder()) {
      ASSERT_TRUE(placeholder_);
      EXPECT_FALSE(placeholder_->GetVisible());
    }
  }

  bool has_placeholder() const { return GetParam(); }

 private:
  // HoldingSpaceTrayChildBubbleTestBase:
  std::unique_ptr<HoldingSpaceTrayChildBubble> CreateChildBubble(
      HoldingSpaceViewDelegate* view_delegate) override {
    return std::make_unique<TestHoldingSpaceTrayChildBubble>(
        view_delegate,
        TestHoldingSpaceTrayChildBubble::Params(
            base::BindOnce(
                &HoldingSpaceTrayChildBubblePlaceholderTest::CreateSections,
                base::Unretained(this)),
            base::BindOnce(
                &HoldingSpaceTrayChildBubblePlaceholderTest::CreatePlaceholder,
                base::Unretained(this))));
  }

  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections(
      HoldingSpaceViewDelegate* view_delegate) {
    auto section = std::make_unique<TestHoldingSpaceItemViewsSection>(
        view_delegate, HoldingSpaceSectionId::kPinnedFiles);
    section_ = section.get();
    std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
    sections.push_back(std::move(section));
    return sections;
  }

  std::unique_ptr<views::View> CreatePlaceholder() {
    auto placeholder =
        has_placeholder() ? std::make_unique<views::View>() : nullptr;
    placeholder_ = placeholder.get();
    return placeholder;
  }

  // Owned by view hierarchy.
  raw_ptr<views::View, DanglingUntriaged> placeholder_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> section_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceTrayChildBubblePlaceholderTest,
                         /*has_placeholder=*/testing::Bool());

// Tests -----------------------------------------------------------------------

TEST_P(HoldingSpaceTrayChildBubblePlaceholderTest,
       MaybeShowsPlaceholderWhenEmpty) {
  {
    SCOPED_TRACE(testing::Message() << "Initial state.");
    ExpectPlaceholderOrGone();
  }

  AddPartiallyInitializedItem(HoldingSpaceItem::Type::kPinnedFile,
                              base::FilePath("foo"));

  {
    SCOPED_TRACE(testing::Message() << "Partially initialized state.");
    ExpectPlaceholderOrGone();
  }

  AddItem(HoldingSpaceItem::Type::kPinnedFile, base::FilePath("foo"));

  {
    SCOPED_TRACE(testing::Message() << "Populated state.");
    ExpectSection();
  }

  RemoveAllItems();

  {
    SCOPED_TRACE(testing::Message() << "Empty state.");
    ExpectPlaceholderOrGone();
  }
}

}  // namespace ash
