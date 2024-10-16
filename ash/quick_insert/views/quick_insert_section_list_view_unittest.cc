// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_section_list_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"
#include "ash/quick_insert/views/quick_insert_gif_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr int kDefaultSectionWidth = 320;

std::unique_ptr<PickerImageItemView> CreateGifItem(
    const gfx::Size& gif_dimensions) {
  return std::make_unique<PickerImageItemView>(
      std::make_unique<PickerGifView>(
          /*frames_fetcher=*/base::DoNothing(),
          /*preview_image_fetcher=*/base::DoNothing(), gif_dimensions),
      u"gif", base::DoNothing());
}

using QuickInsertSectionListViewTest = views::ViewsTestBase;

TEST_F(QuickInsertSectionListViewTest, AddsSection) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section = section_list.AddSection();

  EXPECT_THAT(section_list.children(), ElementsAre(section));
}

TEST_F(QuickInsertSectionListViewTest, ClearsSectionList) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  section_list.AddSection();
  section_list.ClearSectionList();

  EXPECT_THAT(section_list.children(), IsEmpty());
}

TEST_F(QuickInsertSectionListViewTest, GetsTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  QuickInsertItemView* top_item = section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertSectionView* section2 = section_list.AddSection();
  section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(QuickInsertSectionListViewTest, AddsSectionAtTheTop) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertSectionView* section2 = section_list.AddSectionAt(0);
  QuickInsertItemView* top_item = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(QuickInsertSectionListViewTest, GetsTopItemWhenTopSectionIsEmpty) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section = section_list.AddSection();
  QuickInsertItemView* top_item = section->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  section->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  // Add an empty section at the top.
  section_list.AddSectionAt(0);

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(QuickInsertSectionListViewTest, EmptySectionListHasNoTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  EXPECT_EQ(section_list.GetTopItem(), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, GetsBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertSectionView* section2 = section_list.AddSection();
  QuickInsertItemView* bottom_item = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetBottomItem(), bottom_item);
}

TEST_F(QuickInsertSectionListViewTest, GetsBottomItemWhenBottomSectionIsEmpty) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section = section_list.AddSection();
  QuickInsertItemView* top_item = section->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  section->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  // Add an empty section at the bottom.
  section_list.AddSection();

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(QuickInsertSectionListViewTest, EmptySectionListHasNoBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  EXPECT_EQ(section_list.GetBottomItem(), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, GetsItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  QuickInsertItemView* item1 = section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertItemView* item2 = section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertSectionView* section2 = section_list.AddSection();
  QuickInsertItemView* item3 = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_list.GetItemAbove(item2), item1);
  EXPECT_EQ(section_list.GetItemAbove(item3), item2);
}

TEST_F(QuickInsertSectionListViewTest, ItemNotInSectionListHasNoItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  QuickInsertListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemAbove(&item_not_in_section_list), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, GetsItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  QuickInsertItemView* item1 = section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertItemView* item2 = section1->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertSectionView* section2 = section_list.AddSection();
  QuickInsertItemView* item3 = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemBelow(item1), item2);
  EXPECT_EQ(section_list.GetItemBelow(item2), item3);
  EXPECT_EQ(section_list.GetItemBelow(item3), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, ItemNotInSectionListHasNoItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  QuickInsertListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemBelow(&item_not_in_section_list), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, GetsItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  QuickInsertItemView* item1 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertSectionView* section2 = section_list.AddSection();
  QuickInsertItemView* item3 = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_list.GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_list.GetItemLeftOf(item3), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, ItemNotInSectionListHasNoItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  QuickInsertListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemLeftOf(&item_not_in_section_list), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, GetsItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  QuickInsertSectionView* section1 = section_list.AddSection();
  QuickInsertItemView* item1 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertSectionView* section2 = section_list.AddSection();
  QuickInsertItemView* item3 = section2->AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemRightOf(item1), item2);
  EXPECT_EQ(section_list.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_list.GetItemRightOf(item3), nullptr);
}

TEST_F(QuickInsertSectionListViewTest, ItemNotInSectionListHasNoItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  QuickInsertListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemRightOf(&item_not_in_section_list), nullptr);
}

}  // namespace
}  // namespace ash
