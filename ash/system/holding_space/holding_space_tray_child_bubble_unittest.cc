// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

#include <memory>
#include <set>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace {

// TestHoldingSpaceItemViewsSection --------------------------------------------

class TestHoldingSpaceItemViewsSection : public HoldingSpaceItemViewsSection {
 public:
  struct Params {
    std::set<HoldingSpaceItem::Type> supported_types;
    absl::optional<size_t> max_count;
  };

  TestHoldingSpaceItemViewsSection(HoldingSpaceViewDelegate* view_delegate,
                                   Params params)
      : HoldingSpaceItemViewsSection(view_delegate,
                                     std::move(params.supported_types),
                                     params.max_count) {}

 private:
  // HoldingSpaceItemViewsSection:
  std::unique_ptr<views::View> CreateHeader() override {
    return std::make_unique<views::View>();
  }

  std::unique_ptr<views::View> CreateContainer() override {
    return std::make_unique<views::View>();
  }

  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override {
    return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
  }
};

// TestHoldingSpaceTrayChildBubble ---------------------------------------------

class TestHoldingSpaceTrayChildBubble : public HoldingSpaceTrayChildBubble {
 public:
  struct Params {
    base::OnceCallback<
        std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>(
            HoldingSpaceViewDelegate* view_delegate)>
        create_sections_callback;

    base::OnceCallback<std::unique_ptr<views::View>()>
        create_placeholder_callback;
  };

  TestHoldingSpaceTrayChildBubble(HoldingSpaceViewDelegate* view_delegate,
                                  Params params)
      : HoldingSpaceTrayChildBubble(view_delegate),
        params_(std::move(params)) {}

 private:
  // HoldingSpaceChildBubble:
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections()
      override {
    EXPECT_TRUE(params_.create_sections_callback)
        << "Sections created more than once.";
    return std::move(params_.create_sections_callback).Run(delegate());
  }

  std::unique_ptr<views::View> CreatePlaceholder() override {
    EXPECT_TRUE(params_.create_sections_callback)
        << "Placeholder created more than once.";
    return std::move(params_.create_placeholder_callback).Run();
  }

  Params params_;
};

}  // namespace

// HoldingSpaceTrayChildBubbleTest ---------------------------------------------

class HoldingSpaceTrayChildBubbleTest : public HoldingSpaceAshTestBase {
 protected:
  const HoldingSpaceTrayChildBubble* child_bubble() const {
    return child_bubble_.get();
  }

 private:
  // HoldingSpaceAshTestBase:
  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();

    view_delegate_ = std::make_unique<HoldingSpaceViewDelegate>(
        /*bubble=*/nullptr);

    child_bubble_ = CreateChildBubble(view_delegate_.get());
    ASSERT_TRUE(child_bubble_);
    child_bubble_->Init();
  }

  void TearDown() override {
    child_bubble_.reset();
    view_delegate_.reset();

    AshTestBase::TearDown();
  }

  // Invoked from `SetUp()` to create the `child_bubble()`.
  virtual std::unique_ptr<HoldingSpaceTrayChildBubble> CreateChildBubble(
      HoldingSpaceViewDelegate* view_delegate) = 0;

  std::unique_ptr<HoldingSpaceViewDelegate> view_delegate_;
  std::unique_ptr<HoldingSpaceTrayChildBubble> child_bubble_;
};

// HoldingSpaceTrayChildBubblePlaceholderTest ----------------------------------

class HoldingSpaceTrayChildBubblePlaceholderTest
    : public HoldingSpaceTrayChildBubbleTest,
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
  // HoldingSpaceTrayChildBubbleTest:
  std::unique_ptr<HoldingSpaceTrayChildBubble> CreateChildBubble(
      HoldingSpaceViewDelegate* view_delegate) override {
    return std::make_unique<TestHoldingSpaceTrayChildBubble>(
        view_delegate,
        TestHoldingSpaceTrayChildBubble::Params{
            .create_sections_callback = base::BindOnce(
                &HoldingSpaceTrayChildBubblePlaceholderTest::CreateSections,
                base::Unretained(this)),
            .create_placeholder_callback = base::BindOnce(
                &HoldingSpaceTrayChildBubblePlaceholderTest::CreatePlaceholder,
                base::Unretained(this)),
        });
  }

  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections(
      HoldingSpaceViewDelegate* view_delegate) {
    auto section = std::make_unique<TestHoldingSpaceItemViewsSection>(
        view_delegate,
        TestHoldingSpaceItemViewsSection::Params{
            .supported_types = {HoldingSpaceItem::Type::kPinnedFile},
            .max_count = 1u,
        });
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
  views::View* placeholder_ = nullptr;
  views::View* section_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceTrayChildBubblePlaceholderTest,
                         /*has_placeholder=*/testing::Bool());

TEST_P(HoldingSpaceTrayChildBubblePlaceholderTest,
       MaybeShowsPlaceholderWhenEmpty) {
  {
    SCOPED_TRACE(testing::Message() << "Initial state.");
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
