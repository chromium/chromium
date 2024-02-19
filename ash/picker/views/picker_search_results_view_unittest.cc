// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

constexpr int kPickerWidth = 320;

using PickerSearchResultsViewTest = AshTestBase;

auto MatchesResultSection(const PickerSearchResults::Section& section) {
  return AllOf(
      Property(&PickerSectionView::title_label_for_testing,
               Property(&views::Label::GetText, Eq(section.heading()))),
      Property(&PickerSectionView::item_views_for_testing,
               SizeIs(section.results().size())));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSections) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  const PickerSearchResults kSearchResults({{
      PickerSearchResults::Section(u"Section 1",
                                   {{PickerSearchResult::Text(u"Result A")}}),
      PickerSearchResults::Section(u"Section 2",
                                   {{PickerSearchResult::Text(u"Result B"),
                                     PickerSearchResult::Text(u"Result C")}}),
  }});
  view.AppendSearchResults(kSearchResults);

  EXPECT_THAT(view.children(), SizeIs(kSearchResults.sections().size()));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSection(kSearchResults.sections()[0])),
                  Pointee(MatchesResultSection(kSearchResults.sections()[1]))));
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResults) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  const PickerSearchResults kSearchResults({{
      PickerSearchResults::Section(u"Section",
                                   {{PickerSearchResult::Text(u"Result")}}),
  }});
  view.AppendSearchResults(kSearchResults);

  view.ClearSearchResults();

  EXPECT_THAT(view.children(), IsEmpty());
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithGif) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  const PickerSearchResults kSearchResults({{PickerSearchResults::Section(
      u"Gif Section",
      {{PickerSearchResult::Gif(
          /*url=*/GURL(), /*preview_image_url=*/GURL(), gfx::Size(),
          /*content_description=*/u"")}})}});
  view.AppendSearchResults(kSearchResults);

  EXPECT_THAT(view.children(), SizeIs(kSearchResults.sections().size()));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSection(kSearchResults.sections()[0]))));
}

TEST_F(PickerSearchResultsViewTest, UpdatesResultsSections) {
  MockPickerAssetFetcher asset_fetcher;
  PickerSearchResultsView view(kPickerWidth, base::DoNothing(), &asset_fetcher);
  const PickerSearchResults kInitialSearchResults({{
      PickerSearchResults::Section(u"Section",
                                   {{PickerSearchResult::Text(u"Result")}}),
  }});
  view.AppendSearchResults(kInitialSearchResults);

  const PickerSearchResults kUpdatedSearchResults({{
      PickerSearchResults::Section(
          u"Updated Section", {{PickerSearchResult::Text(u"Updated Result")}}),
  }});
  view.AppendSearchResults(kUpdatedSearchResults);

  EXPECT_THAT(view.children(), SizeIs(2));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(
          Pointee(MatchesResultSection(kInitialSearchResults.sections()[0])),
          Pointee(MatchesResultSection(kUpdatedSearchResults.sections()[0]))));
}

struct PickerSearchResultTestCase {
  std::string test_name;
  PickerSearchResult result;
};

class PickerSearchResultsViewResultSelectionTest
    : public PickerSearchResultsViewTest,
      public testing::WithParamInterface<PickerSearchResultTestCase> {};

TEST_P(PickerSearchResultsViewResultSelectionTest, LeftClickSelectsResult) {
  const PickerSearchResultTestCase& test_case = GetParam();
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  MockPickerAssetFetcher asset_fetcher;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          kPickerWidth, future.GetCallback(), &asset_fetcher));
  view->AppendSearchResults(PickerSearchResults({{
      PickerSearchResults::Section(u"section", {{test_case.result}}),
  }}));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  views::View* result_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(result_view);

  EXPECT_EQ(future.Get(), test_case.result);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PickerSearchResultsViewResultSelectionTest,
    testing::ValuesIn<PickerSearchResultTestCase>(
        {{"Text", PickerSearchResult::Text(u"result")},
         {"Emoji", PickerSearchResult::Emoji(u"😊")},
         {"Symbol", PickerSearchResult::Symbol(u"♬")},
         {"Emoticon", PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯")},
         {"Gif", PickerSearchResult::Gif(/*url=*/GURL(),
                                         /*preview_image_url=*/GURL(),
                                         gfx::Size(10, 10),
                                         u"cat gif")}}),
    [](const testing::TestParamInfo<
        PickerSearchResultsViewResultSelectionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash
