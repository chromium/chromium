// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <string>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

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
};

using PickerEmojiBarViewTest = views::ViewsTestBase;

TEST_F(PickerEmojiBarViewTest, CreatesSearchResultItems) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth, &asset_fetcher);

  emoji_bar.SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));

  EXPECT_THAT(emoji_bar.item_row_for_testing()->item_views_for_testing(),
              ElementsAre(Truly(&views::IsViewClass<PickerEmojiItemView>),
                          Truly(&views::IsViewClass<PickerSymbolItemView>)));
}

TEST_F(PickerEmojiBarViewTest, ClearsSearchResults) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerEmojiBarView emoji_bar(&mock_delegate, kPickerWidth, &asset_fetcher);
  emoji_bar.SetSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions,
      {{PickerSearchResult::Emoji(u"😊"), PickerSearchResult::Symbol(u"♬")}},
      /*has_more_results=*/false));

  emoji_bar.ClearSearchResults();

  EXPECT_THAT(emoji_bar.item_row_for_testing()->item_views_for_testing(),
              IsEmpty());
}

}  // namespace
}  // namespace ash
