// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_list_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr int kDefaultSectionWidth = 320;

using PickerSectionListViewTest = views::ViewsTestBase;

TEST_F(PickerSectionListViewTest, AddsSection) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section = section_list.AddSection();

  EXPECT_THAT(section_list.children(), ElementsAre(section));
}

TEST_F(PickerSectionListViewTest, ClearsSectionList) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  section_list.AddSection();
  section_list.ClearSectionList();

  EXPECT_THAT(section_list.children(), IsEmpty());
}

TEST_F(PickerSectionListViewTest, GetsTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* top_item = section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, AddsSectionAtTheTop) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  section1->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerSectionView* section2 = section_list.AddSectionAt(0);
  PickerItemView* top_item = section2->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));

  EXPECT_EQ(section_list.GetTopItem(), top_item);
}

TEST_F(PickerSectionListViewTest, EmptySectionListHasNoTopItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  EXPECT_EQ(section_list.GetTopItem(), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* bottom_item = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetBottomItem(), bottom_item);
}

TEST_F(PickerSectionListViewTest, EmptySectionListHasNoBottomItem) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  EXPECT_EQ(section_list.GetBottomItem(), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  PickerItemView* item2 = section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemAbove(item1), nullptr);
  EXPECT_EQ(section_list.GetItemAbove(item2), nullptr);
  EXPECT_EQ(section_list.GetItemAbove(item3), item1);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemAbove) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);
  PickerEmojiItemView item_not_in_section_list(base::DoNothing(), u"😊");

  EXPECT_EQ(section_list.GetItemAbove(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  PickerItemView* item2 = section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemBelow(item1), item3);
  EXPECT_EQ(section_list.GetItemBelow(item2), item3);
  EXPECT_EQ(section_list.GetItemBelow(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemBelow) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);
  PickerEmojiItemView item_not_in_section_list(base::DoNothing(), u"😊");

  EXPECT_EQ(section_list.GetItemBelow(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  PickerItemView* item2 = section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(section_list.GetItemLeftOf(item2), item1);
  EXPECT_EQ(section_list.GetItemLeftOf(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemLeftOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);
  PickerEmojiItemView item_not_in_section_list(base::DoNothing(), u"😊");

  EXPECT_EQ(section_list.GetItemLeftOf(&item_not_in_section_list), nullptr);
}

TEST_F(PickerSectionListViewTest, GetsItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);

  PickerSectionView* section1 = section_list.AddSection();
  PickerItemView* item1 = section1->AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  PickerItemView* item2 = section1->AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));
  PickerSectionView* section2 = section_list.AddSection();
  PickerItemView* item3 = section2->AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(section_list.GetItemRightOf(item1), item2);
  EXPECT_EQ(section_list.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(section_list.GetItemRightOf(item3), nullptr);
}

TEST_F(PickerSectionListViewTest, ItemNotInSectionListHasNoItemRightOf) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSectionListView section_list(kDefaultSectionWidth, &asset_fetcher);
  PickerEmojiItemView item_not_in_section_list(base::DoNothing(), u"😊");

  EXPECT_EQ(section_list.GetItemRightOf(&item_not_in_section_list), nullptr);
}

}  // namespace
}  // namespace ash
