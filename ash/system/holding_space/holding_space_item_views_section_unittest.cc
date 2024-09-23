// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_section.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "ash/system/holding_space/test_holding_space_item_views_section.h"
#include "ash/system/holding_space/test_holding_space_tray_child_bubble.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

std::vector<std::pair<HoldingSpaceSectionId, HoldingSpaceItem::Type>>
GetSectionIdItemTypePairs() {
  std::vector<std::pair<HoldingSpaceSectionId, HoldingSpaceItem::Type>> pairs;

  for (int i = 0; i <= static_cast<int>(HoldingSpaceSectionId::kMaxValue);
       ++i) {
    auto id = static_cast<HoldingSpaceSectionId>(i);
    auto* section = GetHoldingSpaceSection(id);
    DCHECK(section && section->supported_types.size());

    HoldingSpaceItem::Type type = *(section->supported_types.begin());
    pairs.emplace_back(id, type);
  }

  return pairs;
}

}  // namespace

class HoldingSpaceItemViewsSectionTest
    : public HoldingSpaceAshTestBase,
      public testing::WithParamInterface<
          std::pair<HoldingSpaceSectionId, HoldingSpaceItem::Type>> {
 public:
  HoldingSpaceSectionId section_id() const { return GetParam().first; }
  HoldingSpaceItem::Type item_type() const { return GetParam().second; }

  void set_tear_down_asynchronously(bool tear_down_asynchronously) {
    tear_down_asynchronously_ = tear_down_asynchronously;
  }

  HoldingSpaceItemViewsSection* item_views_section() {
    return item_views_section_;
  }

 private:
  // HoldingSpaceAshTestBase
  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(views::Widget::InitParams{
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET});

    view_delegate_ = std::make_unique<HoldingSpaceViewDelegate>(nullptr);

    auto* tray_child_bubble = widget_->GetRootView()->AddChildView(
        std::make_unique<TestHoldingSpaceTrayChildBubble>(
            view_delegate_.get(),
            TestHoldingSpaceTrayChildBubble::Params(base::BindOnce(
                &HoldingSpaceItemViewsSectionTest::CreateSections,
                base::Unretained(this)))));

    tray_child_bubble->Init();
  }

  void TearDown() override {
    if (!tear_down_asynchronously_)
      widget_->CloseNow();

    widget_.reset();
    view_delegate_.reset();

    HoldingSpaceAshTestBase::TearDown();
  }

  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections(
      HoldingSpaceViewDelegate* view_delegate) {
    auto section = std::make_unique<TestHoldingSpaceItemViewsSection>(
        view_delegate, section_id());
    item_views_section_ = section.get();

    std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
    sections.push_back(std::move(section));
    return sections;
  }

  // When this option is true, the test's teardown procedure mimics the one in
  // production code, where widgets are closed asynchronously. Otherwise,
  // teardown is synchronous for simplicity.
  bool tear_down_asynchronously_ = false;

  views::UniqueWidgetPtr widget_;
  std::unique_ptr<HoldingSpaceViewDelegate> view_delegate_;

  raw_ptr<TestHoldingSpaceItemViewsSection, DanglingUntriaged>
      item_views_section_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HoldingSpaceItemViewsSectionTest,
                         testing::ValuesIn(GetSectionIdItemTypePairs()));

// Verifies the items are ordered as expected.
TEST_P(HoldingSpaceItemViewsSectionTest, ItemOrder) {
  const std::optional<size_t> section_max_views =
      GetHoldingSpaceSection(section_id())->max_visible_item_count;

  // Add a number of items.
  std::vector<HoldingSpaceItem*> items;
  for (size_t i = 0; i <= section_max_views.value_or(10); ++i) {
    base::FilePath file_path("/tmp/fake_" + base::NumberToString(i));
    items.emplace_back(AddItem(item_type(), file_path));
  }

  // Reverse the items so that the are the same order that we expect from the
  // views.
  std::reverse(items.begin(), items.end());

  auto views = item_views_section()->GetHoldingSpaceItemViews();

  // The number of views that will appear depends on the section type. If it's
  // not limited or the number of items is fewer than the max, assume all items
  // are shown.
  size_t expected_views_size =
      section_max_views.has_value() && section_max_views.value() < items.size()
          ? section_max_views.value()
          : items.size();

  ASSERT_EQ(expected_views_size, views.size());

  for (size_t i = 0; i < views.size(); ++i) {
    auto* item_view = HoldingSpaceItemView::Cast(views[i]);
    auto* item = items[i];
    EXPECT_EQ(item_view->item(), item);
  }
}

// Verifies that partially initialized items will not show until they are fully
// initialized.
TEST_P(HoldingSpaceItemViewsSectionTest, PartiallyInitializedItemsDontShow) {
  auto* partially_initialized_item =
      AddPartiallyInitializedItem(item_type(), base::FilePath("/tmp/fake1"));
  auto views = item_views_section()->GetHoldingSpaceItemViews();

  EXPECT_EQ(views.size(), 0u);

  AddItem(item_type(), base::FilePath("/tmp/fake2"));
  views = item_views_section()->GetHoldingSpaceItemViews();

  EXPECT_EQ(views.size(), 1u);

  // Once initialized, the item should show a view as normal.
  model()->InitializeOrRemoveItem(
      partially_initialized_item->id(),
      HoldingSpaceFile(
          partially_initialized_item->file().file_path,
          HoldingSpaceFile::FileSystemType::kTest,
          GURL(base::StrCat({"filesystem:", partially_initialized_item->file()
                                                .file_path.BaseName()
                                                .value()}))));

  views = item_views_section()->GetHoldingSpaceItemViews();
  ASSERT_EQ(views.size(), 2u);
  EXPECT_EQ(views[1]->item(), partially_initialized_item);
}

// Verifies that resetting a section allows it to be destroyed asynchronously.
TEST_P(HoldingSpaceItemViewsSectionTest, ResetForAsyncDestruction) {
  const std::optional<size_t> section_max_views =
      GetHoldingSpaceSection(section_id())->max_visible_item_count;

  // Add items to the section.
  for (size_t i = 0; i <= section_max_views.value_or(10); ++i) {
    base::FilePath file_path("/tmp/fake_" + base::NumberToString(i));
    AddItem(item_type(), file_path);
  }

  // In production, each `HoldingSpaceSection` is `Reset()` when the
  // `HoldingSpaceBubble` is destroyed. Because this test creates a
  // `TestHoldingSpaceChildBubble` owned by the standalone `widget_` rather
  // than a `HoldingSpaceBubble`, `item_views_section_` must be `Reset()`
  // directly. Otherwise, when `item_views_section_` is cleaned up as part of
  // `widget_`'s deferred destruction, it would try to access a stale
  // reference to the already-destroyed `view_delegate_`.
  item_views_section()->Reset();
  set_tear_down_asynchronously(true);
}

}  // namespace ash
