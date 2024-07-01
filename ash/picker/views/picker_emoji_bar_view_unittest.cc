// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>
#include <string>

#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_emoji_bar_view_delegate.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::Truly;

constexpr int kPickerWidth = 320;

class MockEmojiBarViewDelegate : public PickerEmojiBarViewDelegate {
 public:
  MOCK_METHOD(void,
              SelectSearchResult,
              (const PickerSearchResult&),
              (override));
  MOCK_METHOD(void, ShowEmojiPicker, (ui::EmojiPickerCategory), (override));
};

class PickerEmojiBarViewTest : public views::ViewsTestBase {
 private:
  // Needed to create icon button ripples.
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerEmojiBarViewTest, CreatesSearchResultItems) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth);

  emoji_bar.SetSearchResults(
      {PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")});

  EXPECT_THAT(emoji_bar.item_row_for_testing()->children(),
              ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerSymbolItemView>)));
}

TEST_F(PickerEmojiBarViewTest, ClearsSearchResults) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth);
  emoji_bar.SetSearchResults(
      {PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")});

  emoji_bar.ClearSearchResults();

  EXPECT_THAT(emoji_bar.item_row_for_testing()->children(), IsEmpty());
}

TEST_F(PickerEmojiBarViewTest, ClickingMoreEmojisButton) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();

  EXPECT_CALL(mock_delegate, ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis))
      .Times(1);

  ViewDrawnWaiter().Wait(emoji_bar->more_emojis_button_for_testing());
  LeftClickOn(*emoji_bar->more_emojis_button_for_testing());
}

TEST_F(PickerEmojiBarViewTest, ClickingGifsButton) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();

  EXPECT_CALL(mock_delegate, ShowEmojiPicker(ui::EmojiPickerCategory::kGifs))
      .Times(1);

  ViewDrawnWaiter().Wait(emoji_bar->gifs_button_for_testing());
  LeftClickOn(*emoji_bar->gifs_button_for_testing());
}

TEST_F(PickerEmojiBarViewTest, GifsButtonHasNoTooltip) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView view(&mock_delegate, kPickerWidth);

  EXPECT_EQ(view.gifs_button_for_testing()->GetTooltipText(), u"");
}

TEST_F(PickerEmojiBarViewTest, GetsTopItem) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();
  emoji_bar->SetSearchResults(
      {PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")});

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(PickerSearchResult::Emoji(u"😊")));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(emoji_bar->GetTopItem()));
}

TEST_F(PickerEmojiBarViewTest, GetsItemLeftOf) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();
  emoji_bar->SetSearchResults(
      {PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")});
  const views::View::Views& emoji_bar_items =
      emoji_bar->item_row_for_testing()->children();
  ASSERT_THAT(emoji_bar_items, SizeIs(2));

  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar_items[0]), nullptr);
  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar_items[1]), emoji_bar_items[0]);
  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar->gifs_button_for_testing()),
            emoji_bar_items[1]);
  EXPECT_EQ(
      emoji_bar->GetItemLeftOf(emoji_bar->more_emojis_button_for_testing()),
      emoji_bar->gifs_button_for_testing());
}

TEST_F(PickerEmojiBarViewTest, GetsItemRightOf) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();
  emoji_bar->SetSearchResults(
      {PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")});
  const views::View::Views& emoji_bar_items =
      emoji_bar->item_row_for_testing()->children();
  ASSERT_THAT(emoji_bar_items, SizeIs(2));

  EXPECT_EQ(emoji_bar->GetItemRightOf(emoji_bar_items[0]), emoji_bar_items[1]);
  EXPECT_EQ(emoji_bar->GetItemRightOf(emoji_bar_items[1]),
            emoji_bar->gifs_button_for_testing());
  EXPECT_EQ(emoji_bar->GetItemRightOf(emoji_bar->gifs_button_for_testing()),
            emoji_bar->more_emojis_button_for_testing());
  EXPECT_EQ(
      emoji_bar->GetItemRightOf(emoji_bar->more_emojis_button_for_testing()),
      nullptr);
}

}  // namespace
}  // namespace ash
