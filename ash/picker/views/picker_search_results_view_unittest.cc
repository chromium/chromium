// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <string>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_skeleton_loader_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SizeIs;

constexpr int kPickerWidth = 320;

class PickerSearchResultsViewTest : public views::ViewsTestBase {
 public:
  PickerSearchResultsViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

template <class V, class Matcher>
auto AsView(Matcher matcher) {
  return ResultOf(
      "AsViewClass",
      [](views::View* view) { return views::AsViewClass<V>(view); },
      Pointee(matcher));
}

auto MatchesTitlelessSection(int num_items) {
  return AllOf(
      Property(&PickerSectionView::title_label_for_testing, nullptr),
      Property(&PickerSectionView::item_views_for_testing, SizeIs(num_items)));
}

auto MatchesResultSection(PickerSectionType section_type, int num_items) {
  return AllOf(
      Property(&PickerSectionView::title_label_for_testing,
               Property(&views::Label::GetText,
                        GetSectionTitleForPickerSectionType(section_type))),
      Property(&PickerSectionView::item_views_for_testing, SizeIs(num_items)));
}

template <class Matcher>
auto MatchesResultSectionWithOneItem(PickerSectionType section_type,
                                     Matcher item_matcher) {
  return AllOf(
      Property(&PickerSectionView::title_label_for_testing,
               Property(&views::Label::GetText,
                        GetSectionTitleForPickerSectionType(section_type))),
      Property(&PickerSectionView::item_views_for_testing,
               ElementsAre(item_matcher)));
}

class MockSearchResultsViewDelegate : public PickerSearchResultsViewDelegate {
 public:
  MOCK_METHOD(void, SelectMoreResults, (PickerSectionType), (override));
  MOCK_METHOD(void,
              SelectSearchResult,
              (const PickerSearchResult&),
              (override));
  MOCK_METHOD(void, RequestPseudoFocus, (views::View*), (override));
  MOCK_METHOD(PickerActionType,
              GetActionForResult,
              (const PickerSearchResult& result),
              (override));
};

TEST_F(PickerSearchResultsViewTest, CreatesResultsSections) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kNone,
                                 {{PickerSearchResult::Text(u"Result A"),
                                   PickerSearchResult::Text(u"Result B")}},
                                 /*has_more_results=*/false));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerSearchResult::LocalFile(u"Result C", base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(2)),
                          Pointee(MatchesResultSection(
                              PickerSectionType::kLocalFiles, 1))));
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResultsClearsView) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard, {{PickerSearchResult::Text(u"Result")}},
      /*has_more_results=*/false));

  view.ClearSearchResults();

  EXPECT_THAT(view.section_list_view_for_testing()->children(), IsEmpty());
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithCategories) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerSearchResult::Category(PickerCategory::kExpressions)}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(1))));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithLocalFiles) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerSearchResult::LocalFile(u"local", base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSectionWithOneItem(
          PickerSectionType::kLocalFiles,
          AsView<PickerListItemView>(Property(
              &PickerListItemView::GetPrimaryTextForTesting, u"local"))))));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithDriveFiles) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerSearchResult::DriveFile(u"drive", GURL(), base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(
      view.section_views_for_testing(),
      ElementsAre(Pointee(MatchesResultSectionWithOneItem(
          PickerSectionType::kLocalFiles,
          AsView<PickerListItemView>(Property(
              &PickerListItemView::GetPrimaryTextForTesting, u"drive"))))));
}

TEST_F(PickerSearchResultsViewTest, UpdatesResultsSections) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerSearchResult::LocalFile(u"Result", base::FilePath())}},
      /*has_more_results=*/false));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone, {{PickerSearchResult::Text(u"New Result")}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesResultSection(
                              PickerSectionType::kLocalFiles, 1)),
                          Pointee(MatchesTitlelessSection(1))));
}

TEST_F(PickerSearchResultsViewTest, GetsTopItem) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(PickerSearchResult::Text(u"Result A")));

  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kClipboard,
                                 {{PickerSearchResult::Text(u"Result A"),
                                   PickerSearchResult::Text(u"Result B")}},
                                 /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetTopItem()));
}

TEST_F(PickerSearchResultsViewTest, GetsBottomItem) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(PickerSearchResult::Text(u"Result B")));

  view.AppendSearchResults(
      PickerSearchResultsSection(PickerSectionType::kClipboard,
                                 {{PickerSearchResult::Text(u"Result A"),
                                   PickerSearchResult::Text(u"Result B")}},
                                 /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetBottomItem()));
}

TEST_F(PickerSearchResultsViewTest, GetsItemAbove) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerSearchResult::Category(PickerCategory::kLinks),
        PickerSearchResult::Category(PickerCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(
      mock_delegate,
      SelectSearchResult(PickerSearchResult::Category(PickerCategory::kLinks)));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(
      view.GetItemAbove(view.GetBottomItem())));
}

TEST_F(PickerSearchResultsViewTest, GetsItemBelow) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerSearchResult::Category(PickerCategory::kLinks),
        PickerSearchResult::Category(PickerCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(mock_delegate, SelectSearchResult(PickerSearchResult::Category(
                                 PickerCategory::kClipboard)));

  EXPECT_TRUE(
      DoPickerPseudoFocusedActionOnView(view.GetItemBelow(view.GetTopItem())));
}

TEST_F(PickerSearchResultsViewTest, ShowsSeeMoreLinkWhenThereAreMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller));

  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/true));

  ASSERT_THAT(
      view->section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title_trailing_link_for_testing",
          &PickerSectionView::title_trailing_link_for_testing,
          Property(&views::View::GetAccessibleName, u"Show all Files")))));
}

TEST_F(PickerSearchResultsViewTest,
       DoesNotShowSeeMoreLinkWhenThereAreNoMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller));

  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/false));

  ASSERT_THAT(
      view->section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title_trailing_link_for_testing",
          &PickerSectionView::title_trailing_link_for_testing, IsNull()))));
}

TEST_F(PickerSearchResultsViewTest, ClickingSeeMoreLinkCallsCallback) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller));
  widget->Show();
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/true));

  EXPECT_CALL(mock_delegate, SelectMoreResults(PickerSectionType::kLocalFiles));

  views::View* trailing_link =
      view->section_views_for_testing()[0]->title_trailing_link_for_testing();
  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(*trailing_link);
}

TEST_F(PickerSearchResultsViewTest,
       SearchStoppedShowsNoResultsViewWithNoIllustration) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  EXPECT_TRUE(view.SearchStopped(/*illustration=*/{}, u"no results"));

  EXPECT_FALSE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_illustration_for_testing().GetVisible());
  EXPECT_TRUE(view.no_results_label_for_testing().GetVisible());
  EXPECT_EQ(view.no_results_label_for_testing().GetText(), u"no results");
}

TEST_F(PickerSearchResultsViewTest,
       SearchStoppedShowsNoResultsViewWithIllustration) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  EXPECT_TRUE(view.SearchStopped(
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(1)),
      u"no results"));

  EXPECT_FALSE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_illustration_for_testing().GetVisible());
  EXPECT_TRUE(view.no_results_label_for_testing().GetVisible());
  EXPECT_EQ(view.no_results_label_for_testing().GetText(), u"no results");
}

TEST_F(PickerSearchResultsViewTest,
       SearchStoppedShowsSectionListIfThereAreResults) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/true));
  EXPECT_FALSE(view.SearchStopped({}, u""));

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(PickerSearchResultsViewTest, SearchStoppedHidesLoaderView) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.ShowLoadingAnimation();
  ASSERT_TRUE(view.SearchStopped({}, u""));

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResultsShowsSearchResults) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  ASSERT_TRUE(view.SearchStopped({}, u""));

  view.ClearSearchResults();

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(PickerSearchResultsViewTest, ShowLoadingShowsLoaderView) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.ShowLoadingAnimation();

  EXPECT_TRUE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerSearchResultsViewTest, ShowLoadingAnimatesAfterDelay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);

  view.ShowLoadingAnimation();
  task_environment()->FastForwardBy(
      PickerSearchResultsView::kLoadingAnimationDelay);

  EXPECT_TRUE(view.skeleton_loader_view_for_testing()
                  .layer()
                  ->GetAnimator()
                  ->is_animating());
}

TEST_F(PickerSearchResultsViewTest, AppendResultsDuringLoadingStopsAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  task_environment()->FastForwardBy(
      PickerSearchResultsView::kLoadingAnimationDelay);

  view.AppendSearchResults({PickerSearchResultsSection(
      PickerSectionType::kLinks, {PickerSearchResult::Text(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerSearchResultsViewTest, AppendResultsDuringLoadingAppendsResults) {
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller);
  view.ShowLoadingAnimation();

  view.AppendSearchResults({PickerSearchResultsSection(
      PickerSectionType::kLinks, {PickerSearchResult::Text(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_THAT(view.section_views_for_testing(), SizeIs(1));
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
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  MockSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller));
  widget->Show();
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard, {{test_case.result}},
      /*has_more_results=*/false));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  EXPECT_CALL(mock_delegate, SelectSearchResult(test_case.result));

  PickerItemView* result_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(*result_view);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PickerSearchResultsViewResultSelectionTest,
    testing::ValuesIn<PickerSearchResultTestCase>({
        {"Text", PickerSearchResult::Text(u"result")},
        {"Category",
         PickerSearchResult::Category(PickerCategory::kExpressions)},
        {"LocalFile",
         PickerSearchResult::LocalFile(u"local", base::FilePath())},
        {"DriveFile",
         PickerSearchResult::DriveFile(u"drive", GURL(), base::FilePath())},
    }),
    [](const testing::TestParamInfo<
        PickerSearchResultsViewResultSelectionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash
