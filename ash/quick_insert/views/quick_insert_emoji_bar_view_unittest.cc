// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_emoji_bar_view.h"

#include <memory>
#include <string>

#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::Truly;
using ::testing::VariantWith;

constexpr int kQuickInsertWidth = 320;

template <class V, class Matcher>
auto AsView(Matcher matcher) {
  return ResultOf(
      "AsViewClass",
      [](views::View* view) { return views::AsViewClass<V>(view); },
      Pointee(matcher));
}

class MockEmojiBarViewDelegate : public PickerEmojiBarViewDelegate {
 public:
  MOCK_METHOD(void,
              SelectSearchResult,
              (const QuickInsertSearchResult&),
              (override));
  MOCK_METHOD(void, ToggleGifs, (), (override));
  MOCK_METHOD(void, ShowEmojiPicker, (ui::EmojiPickerCategory), (override));
};

class QuickInsertEmojiBarViewTest : public views::ViewsTestBase {
 private:
  // Needed to create icon button ripples.
  AshColorProvider ash_color_provider_;
};

TEST_F(QuickInsertEmojiBarViewTest, HasGridRole) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  EXPECT_EQ(emoji_bar.GetAccessibleRole(), ax::mojom::Role::kGrid);
}

TEST_F(QuickInsertEmojiBarViewTest, HasAccessibleNameWithGifsEnabled) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth,
                               /*is_gifs_enabled=*/true);

  EXPECT_EQ(emoji_bar.GetAccessibleName(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_EMOJI_BAR_WITH_GIFS_GRID_ACCESSIBLE_NAME));
}

TEST_F(QuickInsertEmojiBarViewTest, HasAccessibleNameWithGifsDisabled) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth,
                               /*is_gifs_enabled=*/false);

  EXPECT_EQ(
      emoji_bar.GetAccessibleName(),
      l10n_util::GetStringUTF16(IDS_PICKER_EMOJI_BAR_GRID_ACCESSIBLE_NAME));
}

TEST_F(QuickInsertEmojiBarViewTest, HasSingleChildRowRole) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  EXPECT_THAT(emoji_bar.children(),
              ElementsAre(Pointee(Property(&views::View::GetAccessibleRole,
                                           ax::mojom::Role::kRow))));
}

TEST_F(QuickInsertEmojiBarViewTest, CreatesSearchResultItems) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  emoji_bar.SetSearchResults(
      {QuickInsertEmojiResult::Emoji(u"😊"),
       QuickInsertEmojiResult::Symbol(u"♬"),
       QuickInsertEmojiResult::Emoticon(u"(°□°)", u"surprise")});

  EXPECT_THAT(emoji_bar.GetItemsForTesting(),
              ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerEmojiItemView>)));
}

TEST_F(QuickInsertEmojiBarViewTest, SearchResultsWithNamesHaveTooltips) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  emoji_bar.SetSearchResults(
      {QuickInsertEmojiResult::Emoji(u"😊", u"happy"),
       QuickInsertEmojiResult::Symbol(u"♬", u"music"),
       QuickInsertEmojiResult::Emoticon(u"(°□°)", u"surprise")});

  EXPECT_THAT(
      emoji_bar.GetItemsForTesting(),
      ElementsAre(AsView<views::Button>(
                      Property(&views::Button::GetTooltipText, u"happy emoji")),
                  AsView<views::Button>(
                      Property(&views::Button::GetTooltipText, u"music")),
                  AsView<views::Button>(Property(&views::Button::GetTooltipText,
                                                 u"surprise emoticon"))));
}

TEST_F(QuickInsertEmojiBarViewTest, SearchResultsWithNamesHaveAccessibleNames) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  emoji_bar.SetSearchResults(
      {QuickInsertEmojiResult::Emoji(u"😊", u"happy"),
       QuickInsertEmojiResult::Symbol(u"♬", u"music"),
       QuickInsertEmojiResult::Emoticon(u"(°□°)", u"surprise")});

  EXPECT_THAT(
      emoji_bar.GetItemsForTesting(),
      ElementsAre(
          Pointee(Property(&views::View::GetAccessibleName, u"happy emoji")),
          Pointee(Property(&views::View::GetAccessibleName, u"music")),
          Pointee(Property(&views::View::GetAccessibleName,
                           u"surprise emoticon"))));
}

TEST_F(QuickInsertEmojiBarViewTest, SearchResultsWithNoNameHaveNoTooltips) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  emoji_bar.SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                              QuickInsertEmojiResult::Symbol(u"♬"),
                              QuickInsertEmojiResult::Emoticon(u"(°□°)")});

  EXPECT_THAT(
      emoji_bar.GetItemsForTesting(),
      ElementsAre(
          AsView<views::Button>(Property(&views::Button::GetTooltipText, u"")),
          AsView<views::Button>(Property(&views::Button::GetTooltipText, u"")),
          AsView<views::Button>(
              Property(&views::Button::GetTooltipText, u""))));
}

TEST_F(QuickInsertEmojiBarViewTest,
       SearchResultsWithNoNamesUseLabelAsAccessibleName) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);

  emoji_bar.SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                              QuickInsertEmojiResult::Symbol(u"♬"),
                              QuickInsertEmojiResult::Emoticon(u"(°□°)")});

  EXPECT_THAT(
      emoji_bar.GetItemsForTesting(),
      ElementsAre(
          Pointee(Property(&views::View::GetAccessibleName, u"😊")),
          Pointee(Property(&views::View::GetAccessibleName, u"♬")),
          Pointee(Property(&views::View::GetAccessibleName, u"(°□°)"))));
}

TEST_F(QuickInsertEmojiBarViewTest, ClearsSearchResults) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kQuickInsertWidth);
  emoji_bar.SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                              QuickInsertEmojiResult::Symbol(u"♬")});

  emoji_bar.ClearSearchResults();

  EXPECT_THAT(emoji_bar.GetItemsForTesting(), IsEmpty());
}

TEST_F(QuickInsertEmojiBarViewTest, ClickingMoreEmojisButton) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kQuickInsertWidth));
  widget->Show();

  EXPECT_CALL(mock_delegate, ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis))
      .Times(1);

  ViewDrawnWaiter().Wait(emoji_bar->more_emojis_button_for_testing());
  LeftClickOn(*emoji_bar->more_emojis_button_for_testing());
}

TEST_F(QuickInsertEmojiBarViewTest, MoreEmojisButtonHasTooltipWithGifsEnabled) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView view(&mock_delegate, kQuickInsertWidth,
                          /*is_gifs_enabled=*/true);

  EXPECT_EQ(view.more_emojis_button_for_testing()->GetTooltipText(),
            l10n_util::GetStringUTF16(
                IDS_PICKER_MORE_EMOJIS_AND_GIFS_BUTTON_ACCESSIBLE_NAME));
}

TEST_F(QuickInsertEmojiBarViewTest,
       MoreEmojisButtonHasTooltipWithGifsDisabled) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView view(&mock_delegate, kQuickInsertWidth,
                          /*is_gifs_enabled=*/false);

  EXPECT_EQ(
      view.more_emojis_button_for_testing()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
}

TEST_F(QuickInsertEmojiBarViewTest, ClickingGifsButton) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/true));
  widget->Show();

  EXPECT_CALL(mock_delegate, ToggleGifs()).Times(1);

  ViewDrawnWaiter().Wait(emoji_bar->gifs_button_for_testing());
  LeftClickOn(*emoji_bar->gifs_button_for_testing());
}

TEST_F(QuickInsertEmojiBarViewTest, GifsButtonNotVisibleWhenDisabled) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/false));
  widget->Show();

  EXPECT_FALSE(emoji_bar->gifs_button_for_testing()->GetVisible());
}

TEST_F(QuickInsertEmojiBarViewTest, GifsButtonHasNoTooltip) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView view(&mock_delegate, kQuickInsertWidth,
                          /*is_gifs_enabled=*/true);

  EXPECT_EQ(view.gifs_button_for_testing()->GetTooltipText(), u"");
}

TEST_F(QuickInsertEmojiBarViewTest, GetsTopItem) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kQuickInsertWidth));
  widget->Show();
  emoji_bar->SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                               QuickInsertEmojiResult::Symbol(u"♬")});

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<QuickInsertEmojiResult>(
                  QuickInsertEmojiResult::Emoji(u"😊"))));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(emoji_bar->GetTopItem()));
}

TEST_F(QuickInsertEmojiBarViewTest, GetsItemLeftOf) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/true));
  widget->Show();
  emoji_bar->SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                               QuickInsertEmojiResult::Symbol(u"♬")});
  const views::View::Views& emoji_bar_items = emoji_bar->GetItemsForTesting();
  ASSERT_THAT(emoji_bar_items, SizeIs(2));

  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar_items[0]), nullptr);
  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar_items[1]), emoji_bar_items[0]);
  EXPECT_EQ(emoji_bar->GetItemLeftOf(emoji_bar->gifs_button_for_testing()),
            emoji_bar_items[1]);
  EXPECT_EQ(
      emoji_bar->GetItemLeftOf(emoji_bar->more_emojis_button_for_testing()),
      emoji_bar->gifs_button_for_testing());
}

TEST_F(QuickInsertEmojiBarViewTest, GetsItemLeftOfSkipsGifsIfGifsDisabled) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/false));
  widget->Show();
  emoji_bar->SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊")});
  const views::View::Views& emoji_bar_items = emoji_bar->GetItemsForTesting();
  ASSERT_THAT(emoji_bar_items, SizeIs(1));

  EXPECT_EQ(
      emoji_bar->GetItemLeftOf(emoji_bar->more_emojis_button_for_testing()),
      emoji_bar_items[0]);
}

TEST_F(QuickInsertEmojiBarViewTest, GetsItemRightOf) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/true));
  widget->Show();
  emoji_bar->SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                               QuickInsertEmojiResult::Symbol(u"♬")});
  const views::View::Views& emoji_bar_items = emoji_bar->GetItemsForTesting();
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

TEST_F(QuickInsertEmojiBarViewTest, GetsItemRightOfSkipsGifsIfGifsDisabled) {
  MockEmojiBarViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar =
      widget->SetContentsView(std::make_unique<PickerEmojiBarView>(
          &mock_delegate, kQuickInsertWidth, /*is_gifs_enabled=*/false));
  widget->Show();
  emoji_bar->SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊")});
  const views::View::Views& emoji_bar_items = emoji_bar->GetItemsForTesting();
  ASSERT_THAT(emoji_bar_items, SizeIs(1));

  EXPECT_EQ(emoji_bar->GetItemRightOf(emoji_bar_items[0]),
            emoji_bar->more_emojis_button_for_testing());
}

TEST_F(QuickInsertEmojiBarViewTest, ItemsAreTruncatedToFit) {
  MockEmojiBarViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, 200);

  emoji_bar.SetSearchResults({QuickInsertEmojiResult::Emoji(u"😊"),
                              QuickInsertEmojiResult::Emoji(u"😊"),
                              QuickInsertEmojiResult::Emoji(u"😊")});

  EXPECT_EQ(emoji_bar.GetNumItems(), 2u);
  EXPECT_THAT(emoji_bar.GetItemsForTesting(),
              ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerEmojiItemView>)));
}

}  // namespace
}  // namespace ash
