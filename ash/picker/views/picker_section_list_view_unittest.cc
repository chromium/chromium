// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_list_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_submenu_controller.h"
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

using PickerSectionListViewTest = views::ViewsTestBase;

TEST_F(PickerSectionListViewTest, AddsSection) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section = section_list.AddSection();

  EXPECT_THAT(section_list.children(), ElementsAre(section));
}

TEST_F(PickerSectionListViewTest, ClearsSectionList) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  section_list.AddSection();
  section_list.ClearSectionList();

  EXPECT_THAT(section_list.children(), IsEmpty());
}

TEST_F(PickerSectionListViewTest, GetsTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* top_item = section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSection();
  section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, AddsSectionAtTheTop) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSectionAt(0);
  PickerItemView* top_item = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, GetsTopItemWhenTopSectionIsEmpty) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section = section_list.AddSection();
  PickerItemView* top_item = section->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  section->AddListItem(std::make_unique<PickerListItemView>(base::DoNothing()));
  // Add an empty section at the top.
  section_list.AddSectionAt(0);

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, EmptySectionListHasNoTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  EXPECT_EQ(section_list.GetTopItem(), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* bottom_item = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetBottomItem(), bottom_item);
}

TEST_F(PickerSectionListViewTest, GetsBottomItemWhenBottomSectionIsEmpty) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section = section_list.AddSection();
  PickerItemView* top_item = section->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  section->AddListItem(std::make_unique<PickerListItemView>(base::DoNothing()));
  // Add an empty section at the bottom.
  section_list.AddSection();

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, EmptySectionListHasNoBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  EXPECT_EQ(section_list.GetBottomItem(), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerItemView* item2 = section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_list.GetItemAbove(item2), item1);
  EXPECT_EQ(section_list.GetItemAbove(item3), item2);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  PickerListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemAbove(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerItemView* item2 = section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemBelow(item1), item2);
  EXPECT_EQ(section_list.GetItemBelow(item2), item3);
  EXPECT_EQ(section_list.GetItemBelow(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  PickerListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemBelow(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_list.GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_list.GetItemLeftOf(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  PickerListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemLeftOf(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      section1->AddImageGridItem(CreateGifItem(gfx::Size(100, 100)));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemRightOf(item1), item2);
  EXPECT_EQ(section_list.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_list.GetItemRightOf(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher,
                                     &submenu_controller);
  PickerListItemView item_not_in_section_list(base::DoNothing());

  EXPECT_EQ(section_list.GetItemRightOf(&item_not_in_section_list), nullptr);
}

}  // namespace
}  // namespace ash
