// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

constexpr int kPickerWidth = 320;

using PickerSearchResultsViewTest = views::ViewsTestBase;

auto MatchesResultSection(PickerSectionType section_type, int num_items) {
  return AllOf(
      Property(&PickerSectionView::title_label_for_testing,
               Property(&views::Label::GetText,
                        GetSectionTitleForPickerSectionType(section_type))),
      Property(&PickerSectionView::item_views_for_testing, SizeIs(num_items)));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSections) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kExpressions,
                                 {{PickerSearchResult::Text(u"Result A")}}));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLinks, {{PickerSearchResult::Text(u"Result B"),
                                   PickerSearchResult::Text(u"Result C")}}));

  EXPECT_THAT(view.children(), SizeIs(2));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(
          Pointee(MatchesResultSection(PickerSectionType::kExpressions, 1)),
          Pointee(MatchesResultSection(PickerSectionType::kLinks, 2))));
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kExpressions,
                                 {{PickerSearchResult::Text(u"Result")}}));

  view.ClearSearchResults();

  EXPECT_THAT(view.children(), IsEmpty());
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithGif) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kGifs,
      {{PickerSearchResult::Gif(
          /*preview_url=*/GURL(), /*preview_image_url=*/GURL(), gfx::Size(),
          /*full_url=*/GURL(),
          /*content_description=*/u"")}}));

  EXPECT_THAT(view.children(), SizeIs(1));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSection(PickerSectionType::kGifs, 1))));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithCategories) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kCategories,
      {{PickerSearchResult::Category(PickerCategory::kEmojis)}}));

  EXPECT_THAT(view.children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(
                  MatchesResultSection(PickerSectionType::kCategories, 1))));
}

TEST_F(PickerSearchResultsViewTest, UpdatesResultsSections) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kExpressions,
                                 {{PickerSearchResult::Text(u"Result")}}));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLinks,
      {{PickerSearchResult::Text(u"Updated Result")}}));

  EXPECT_THAT(view.children(), SizeIs(2));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(
          Pointee(MatchesResultSection(PickerSectionType::kExpressions, 1)),
          Pointee(MatchesResultSection(PickerSectionType::kLinks, 1))));
}

TEST_F(PickerSearchResultsViewTest,
       PressingEnterDoesNothingForEmptySearchResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);

  EXPECT_FALSE(view.OnEnterKeyPressed());
}

struct PickerSearchResultTestCase {
  std::string test_name;
  PickerSearchResult result;
};

class PickerSearchResultsViewResultSelectionTest
    : public PickerSearchResultsViewTest,
      public testing::WithParamInterface<PickerSearchResultTestCase> {
 private:
  AshColorProvider ash_color_provider_;
};

TEST_P(PickerSearchResultsViewResultSelectionTest, LeftClickSelectsResult) {
  const PickerSearchResultTestCase& test_case = GetParam();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  MockPickerAssetFetcher asset_fetcher;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          kPickerWidth, future.GetCallback(), &asset_fetcher));
  widget->Show();
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions, {{test_case.result}}));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  PickerItemView* result_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(*result_view);

  EXPECT_EQ(future.Get(), test_case.result);
}

TEST_P(PickerSearchResultsViewResultSelectionTest, PressingEnterSelectsResult) {
  const PickerSearchResultTestCase& test_case = GetParam();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  MockPickerAssetFetcher asset_fetcher;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          kPickerWidth, future.GetCallback(), &asset_fetcher));
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kExpressions, {{test_case.result}}));

  EXPECT_TRUE(view->OnEnterKeyPressed());
  EXPECT_EQ(future.Get(), test_case.result);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PickerSearchResultsViewResultSelectionTest,
    testing::ValuesIn<PickerSearchResultTestCase>({
        {"Text", PickerSearchResult::Text(u"result")},
        {"Emoji", PickerSearchResult::Emoji(u"😊")},
        {"Symbol", PickerSearchResult::Symbol(u"♬")},
        {"Emoticon", PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯")},
        {"Gif", PickerSearchResult::Gif(/*preview_url=*/GURL(),
                                        /*preview_image_url=*/GURL(),
                                        gfx::Size(10, 10),
                                        /*full_url=*/GURL(),
                                        u"cat gif")},
        {"Category", PickerSearchResult::Category(PickerCategory::kEmojis)},
    }),
    [](const testing::TestParamInfo<
        PickerSearchResultsViewResultSelectionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash
