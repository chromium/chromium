// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>
#include <string>

#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Truly;

constexpr int kPickerWidth = 320;

class MockSearchResultsViewDelegate : public PickerSearchResultsViewDelegate {
 public:
  MOCK_METHOD(void, SelectMoreResults, (PickerSectionType), (override));
  MOCK_METHOD(void,
              SelectSearchResult,
              (const PickerSearchResult&),
              (override));
  MOCK_METHOD(void, NotifyPseudoFocusChanged, (views::View*), (override));
  MOCK_METHOD(PickerActionType,
              GetActionForResult,
              (const PickerSearchResult&),
              (override));
};

class PickerEmojiBarViewTest : public views::ViewsTestBase {
 private:
  // Needed to create icon button ripples.
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerEmojiBarViewTest, CreatesSearchResultItems) {
  MockSearchResultsViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth);

  emoji_bar.SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));

  EXPECT_THAT(emoji_bar.item_row_for_testing()->children(),
              ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerSymbolItemView>)));
}

TEST_F(PickerEmojiBarViewTest, ClearsSearchResults) {
  MockSearchResultsViewDelegate mock_delegate;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth);
  emoji_bar.SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));

  emoji_bar.ClearSearchResults();

  EXPECT_THAT(emoji_bar.item_row_for_testing()->children(), IsEmpty());
}

TEST_F(PickerEmojiBarViewTest, ClickingMoreEmojisButton) {
  MockSearchResultsViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();

  EXPECT_CALL(mock_delegate, SelectMoreResults(PickerSectionType::kExpressions))
      .Times(1);

  ViewDrawnWaiter().Wait(emoji_bar->more_emojis_button_for_testing());
  LeftClickOn(*emoji_bar->more_emojis_button_for_testing());
}

TEST_F(PickerEmojiBarViewTest, GainsPseudoFocus) {
  MockSearchResultsViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();
  emoji_bar->SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));
  ViewDrawnWaiter().Wait(emoji_bar->item_row_for_testing()->children().front());

  EXPECT_CALL(mock_delegate, NotifyPseudoFocusChanged(_)).Times(1);
  EXPECT_CALL(mock_delegate,
              SelectSearchResult(PickerSearchResult::Emoji(u"😊")));

  EXPECT_TRUE(emoji_bar->GainPseudoFocus(
      PickerPseudoFocusHandler::PseudoFocusDirection::kForward));
  EXPECT_TRUE(emoji_bar->DoPseudoFocusedAction());
}

TEST_F(PickerEmojiBarViewTest, AdvancesPseudoFocus) {
  MockSearchResultsViewDelegate mock_delegate;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* emoji_bar = widget->SetContentsView(
      std::make_unique<PickerEmojiBarView>(&mock_delegate, kPickerWidth));
  widget->Show();
  emoji_bar->SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));
  ViewDrawnWaiter().Wait(emoji_bar->item_row_for_testing()->children().front());

  EXPECT_CALL(mock_delegate, NotifyPseudoFocusChanged(_)).Times(2);
  EXPECT_CALL(mock_delegate,
              SelectSearchResult(PickerSearchResult::Symbol(u"♬")));

  EXPECT_TRUE(emoji_bar->GainPseudoFocus(
      PickerPseudoFocusHandler::PseudoFocusDirection::kForward));
  EXPECT_TRUE(emoji_bar->AdvancePseudoFocus(
      PickerPseudoFocusHandler::PseudoFocusDirection::kForward));
  EXPECT_TRUE(emoji_bar->DoPseudoFocusedAction());
}

}  // namespace
}  // namespace ash
