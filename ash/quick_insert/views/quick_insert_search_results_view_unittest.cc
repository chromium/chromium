// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_search_results_view.h"

#include <optional>
#include <string>

#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/quick_insert/views/mock_quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_section_list_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_skeleton_loader_view.h"
#include "ash/quick_insert/views/quick_insert_strings.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/ax_event_counter.h"
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
using ::testing::VariantWith;

constexpr int kQuickInsertWidth = 320;

class QuickInsertSearchResultsViewTest : public views::ViewsTestBase {
 public:
  QuickInsertSearchResultsViewTest()
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
      Property(&QuickInsertSectionView::title_label_for_testing, nullptr),
      Property(&QuickInsertSectionView::item_views_for_testing,
               SizeIs(num_items)));
}

auto MatchesResultSection(QuickInsertSectionType section_type, int num_items) {
  return AllOf(
      Property(
          &QuickInsertSectionView::title_label_for_testing,
          Property(&views::Label::GetText,
                   GetSectionTitleForQuickInsertSectionType(section_type))),
      Property(&QuickInsertSectionView::item_views_for_testing,
               SizeIs(num_items)));
}

template <class Matcher>
auto MatchesResultSectionWithOneItem(QuickInsertSectionType section_type,
                                     Matcher item_matcher) {
  return AllOf(
      Property(
          &QuickInsertSectionView::title_label_for_testing,
          Property(&views::Label::GetText,
                   GetSectionTitleForQuickInsertSectionType(section_type))),
      Property(&QuickInsertSectionView::item_views_for_testing,
               ElementsAre(item_matcher)));
}

TEST_F(QuickInsertSearchResultsViewTest, CreatesResultsSections) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(
      QuickInsertSearchResultsSection(QuickInsertSectionType::kNone,
                                      {{QuickInsertTextResult(u"Result A"),
                                        QuickInsertTextResult(u"Result B")}},
                                      /*has_more_results=*/false));
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles,
      {{QuickInsertLocalFileResult(u"Result C", base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(2)),
                          Pointee(MatchesResultSection(
                              QuickInsertSectionType::kLocalFiles, 1))));
}

TEST_F(QuickInsertSearchResultsViewTest, ClearSearchResultsClearsView) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"Result")}},
      /*has_more_results=*/false));

  view.ClearSearchResults();

  EXPECT_THAT(view.section_list_view_for_testing()->children(), IsEmpty());
}

TEST_F(QuickInsertSearchResultsViewTest, EmptySearchResultsShowsThrobber) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{QuickInsertTextResult(u"Result")}},
      /*has_more_results=*/false));

  view.ClearSearchResults();

  EXPECT_TRUE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, CreatesResultsSectionWithGif) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone,
      {{QuickInsertGifResult(
          /*preview_url=*/GURL(), /*preview_image_url=*/GURL(), gfx::Size(),
          /*full_url=*/GURL(), gfx::Size(),
          /*content_description=*/u"")}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(1))));
}

TEST_F(QuickInsertSearchResultsViewTest, CreatesResultsSectionWithCategories) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone,
      {{QuickInsertCategoryResult(QuickInsertCategory::kEmojisGifs)}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(1))));
}

TEST_F(QuickInsertSearchResultsViewTest, CreatesResultsSectionWithLocalFiles) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles,
      {{QuickInsertLocalFileResult(u"local", base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesResultSectionWithOneItem(
                  QuickInsertSectionType::kLocalFiles,
                  AsView<QuickInsertListItemView>(Property(
                      &QuickInsertListItemView::GetPrimaryTextForTesting,
                      u"local"))))));
}

TEST_F(QuickInsertSearchResultsViewTest, CreatesResultsSectionWithDriveFiles) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles,
      {{QuickInsertDriveFileResult(/*id=*/std::nullopt, u"drive", GURL(),
                                   base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesResultSectionWithOneItem(
                  QuickInsertSectionType::kLocalFiles,
                  AsView<QuickInsertListItemView>(Property(
                      &QuickInsertListItemView::GetPrimaryTextForTesting,
                      u"drive"))))));
}

TEST_F(QuickInsertSearchResultsViewTest, UpdatesResultsSections) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles,
      {{QuickInsertLocalFileResult(u"Result", base::FilePath())}},
      /*has_more_results=*/false));
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone, {{QuickInsertTextResult(u"New Result")}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesResultSection(
                              QuickInsertSectionType::kLocalFiles, 1)),
                          Pointee(MatchesTitlelessSection(1))));
}

TEST_F(QuickInsertSearchResultsViewTest, GetsTopItem) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<QuickInsertTextResult>(
                  QuickInsertTextResult(u"Result A"))));

  view.AppendSearchResults(
      QuickInsertSearchResultsSection(QuickInsertSectionType::kClipboard,
                                      {{QuickInsertTextResult(u"Result A"),
                                        QuickInsertTextResult(u"Result B")}},
                                      /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetTopItem()));
}

TEST_F(QuickInsertSearchResultsViewTest, GetsBottomItem) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<QuickInsertTextResult>(
                  QuickInsertTextResult(u"Result B"))));

  view.AppendSearchResults(
      QuickInsertSearchResultsSection(QuickInsertSectionType::kClipboard,
                                      {{QuickInsertTextResult(u"Result A"),
                                        QuickInsertTextResult(u"Result B")}},
                                      /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetBottomItem()));
}

TEST_F(QuickInsertSearchResultsViewTest, GetsItemAbove) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone,
      {{QuickInsertCategoryResult(QuickInsertCategory::kLinks),
        QuickInsertCategoryResult(QuickInsertCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<QuickInsertCategoryResult>(
                  QuickInsertCategoryResult(QuickInsertCategory::kLinks))));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(
      view.GetItemAbove(view.GetBottomItem())));
}

TEST_F(QuickInsertSearchResultsViewTest, GetsItemBelow) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone,
      {{QuickInsertCategoryResult(QuickInsertCategory::kLinks),
        QuickInsertCategoryResult(QuickInsertCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<QuickInsertCategoryResult>(
                  QuickInsertCategoryResult(QuickInsertCategory::kClipboard))));

  EXPECT_TRUE(
      DoPickerPseudoFocusedActionOnView(view.GetItemBelow(view.GetTopItem())));
}

TEST_F(QuickInsertSearchResultsViewTest,
       ShowsSeeMoreLinkWhenThereAreMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, &asset_fetcher,
          &submenu_controller, &preview_controller));

  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles, {}, /*has_more_results=*/true));

  ASSERT_THAT(
      view->section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title_trailing_link_for_testing",
          &QuickInsertSectionView::title_trailing_link_for_testing,
          Property(
              &views::View::GetAccessibleName,
              l10n_util::GetStringUTF16(
                  IDS_PICKER_SEE_MORE_LOCAL_FILES_BUTTON_ACCESSIBLE_NAME))))));
}

TEST_F(QuickInsertSearchResultsViewTest,
       DoesNotShowSeeMoreLinkWhenThereAreNoMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, &asset_fetcher,
          &submenu_controller, &preview_controller));

  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles, {}, /*has_more_results=*/false));

  ASSERT_THAT(view->section_views_for_testing(),
              ElementsAre(Pointee(Property(
                  "title_trailing_link_for_testing",
                  &QuickInsertSectionView::title_trailing_link_for_testing,
                  IsNull()))));
}

TEST_F(QuickInsertSearchResultsViewTest, ClickingSeeMoreLinkCallsCallback) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, &asset_fetcher,
          &submenu_controller, &preview_controller));
  widget->Show();
  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles, {}, /*has_more_results=*/true));

  EXPECT_CALL(mock_delegate,
              SelectMoreResults(QuickInsertSectionType::kLocalFiles));

  views::View* trailing_link =
      view->section_views_for_testing()[0]->title_trailing_link_for_testing();
  ViewDrawnWaiter().Wait(trailing_link);
  LeftClickOn(*trailing_link);
}

TEST_F(QuickInsertSearchResultsViewTest,
       SearchStoppedShowsNoResultsViewWithNoIllustration) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  EXPECT_TRUE(view.SearchStopped(/*illustration=*/{}, u"no results"));

  EXPECT_FALSE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_illustration_for_testing().GetVisible());
  EXPECT_TRUE(view.no_results_label_for_testing().GetVisible());
  EXPECT_EQ(view.no_results_label_for_testing().GetText(), u"no results");
}

TEST_F(QuickInsertSearchResultsViewTest,
       SearchStoppedShowsNoResultsViewWithIllustration) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  EXPECT_TRUE(view.SearchStopped(
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(1)),
      u"no results"));

  EXPECT_FALSE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_illustration_for_testing().GetVisible());
  EXPECT_TRUE(view.no_results_label_for_testing().GetVisible());
  EXPECT_EQ(view.no_results_label_for_testing().GetText(), u"no results");
}

TEST_F(QuickInsertSearchResultsViewTest,
       SearchStoppedShowsSectionListIfThereAreResults) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLocalFiles, {}, /*has_more_results=*/true));
  EXPECT_FALSE(view.SearchStopped({}, u""));

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, SearchStoppedHidesLoaderView) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.ShowLoadingAnimation();
  ASSERT_TRUE(view.SearchStopped({}, u""));

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, SearchStoppedHidesThrobber) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.ClearSearchResults();
  ASSERT_TRUE(view.SearchStopped({}, u""));

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, ClearSearchResultsShowsSearchResults) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  ASSERT_TRUE(view.SearchStopped({}, u""));

  view.ClearSearchResults();

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, ShowLoadingShowsLoaderView) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.ShowLoadingAnimation();

  EXPECT_TRUE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(QuickInsertSearchResultsViewTest, ShowSkeletonLoaderHidesThrobber) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.ClearSearchResults();
  view.ShowLoadingAnimation();

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest, ShowLoadingAnimatesAfterDelay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.ShowLoadingAnimation();
  task_environment()->FastForwardBy(
      QuickInsertSearchResultsView::kLoadingAnimationDelay);

  EXPECT_TRUE(view.skeleton_loader_view_for_testing()
                  .layer()
                  ->GetAnimator()
                  ->is_animating());
}

TEST_F(QuickInsertSearchResultsViewTest,
       AppendResultsDuringLoadingStopsAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  task_environment()->FastForwardBy(
      QuickInsertSearchResultsView::kLoadingAnimationDelay);

  view.AppendSearchResults({QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLinks, {QuickInsertTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(QuickInsertSearchResultsViewTest,
       AppendResultsDuringLoadingAppendsResults) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);
  view.ShowLoadingAnimation();

  view.AppendSearchResults({QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLinks, {QuickInsertTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_THAT(view.section_views_for_testing(), SizeIs(1));
}

TEST_F(QuickInsertSearchResultsViewTest, AppendResultsHidesThrobber) {
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  QuickInsertSearchResultsView view(&mock_delegate, kQuickInsertWidth,
                                    &asset_fetcher, &submenu_controller,
                                    &preview_controller);

  view.AppendSearchResults({QuickInsertSearchResultsSection(
      QuickInsertSectionType::kLinks, {QuickInsertTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(QuickInsertSearchResultsViewTest,
       StoppingSearchDoesNotAnnounceWhenThereAreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone, {}, /*has_more_results=*/false));
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(), u"");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 0);
}

TEST_F(QuickInsertSearchResultsViewTest,
       StoppingSearchWithNoEmojisTriggersLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_TRUE(view->SearchStopped(/*illustration=*/{}, u""));

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(QuickInsertSearchResultsViewTest,
       StoppingSearchWithEmojisTriggersLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));
  view->SetNumEmojiResultsForA11y(5);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_TRUE(view->SearchStopped(/*illustration=*/{}, u""));

  EXPECT_EQ(view->GetAccessibleName(), u"5 emojis. No other results.");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(QuickInsertSearchResultsViewTest,
       StoppingSearchConsecutivelyDoesNotTriggerLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SearchStopped(/*illustration=*/{}, u"");
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(QuickInsertSearchResultsViewTest,
       StoppingSearchAfterClearingSearchAnnouncesWhenThereAreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SearchStopped(/*illustration=*/{}, u"");
  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kNone, {}, /*has_more_results=*/false));
  view->ClearSearchResults();
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 2);
}

TEST_F(QuickInsertSearchResultsViewTest, ClearingSearchResultsDoesNotAnnounce) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->ClearSearchResults();

  EXPECT_EQ(view->GetAccessibleName(), u"");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 0);
}

struct QuickInsertSearchResultTestCase {
  std::string test_name;
  QuickInsertSearchResult result;
};

class QuickInsertSearchResultsViewResultSelectionTest
    : public QuickInsertSearchResultsViewTest,
      public testing::WithParamInterface<QuickInsertSearchResultTestCase> {
 private:
  AshColorProvider ash_color_provider_;
};

TEST_P(QuickInsertSearchResultsViewResultSelectionTest,
       LeftClickSelectsResult) {
  const QuickInsertSearchResultTestCase& test_case = GetParam();
  MockQuickInsertSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view =
      widget->SetContentsView(std::make_unique<QuickInsertSearchResultsView>(
          &mock_delegate, kQuickInsertWidth, &asset_fetcher,
          &submenu_controller, &preview_controller));
  widget->Show();
  view->AppendSearchResults(QuickInsertSearchResultsSection(
      QuickInsertSectionType::kClipboard, {{test_case.result}},
      /*has_more_results=*/false));
  ASSERT_THAT(view->section_views_for_testing(), Not(IsEmpty()));
  ASSERT_THAT(view->section_views_for_testing()[0]->item_views_for_testing(),
              Not(IsEmpty()));

  EXPECT_CALL(mock_delegate, SelectSearchResult(test_case.result));

  QuickInsertItemView* result_view =
      view->section_views_for_testing()[0]->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(result_view);
  LeftClickOn(*result_view);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    QuickInsertSearchResultsViewResultSelectionTest,
    testing::ValuesIn<QuickInsertSearchResultTestCase>({
        {"Text", QuickInsertTextResult(u"result")},
        {"Gif", QuickInsertGifResult(
                    /*preview_url=*/GURL(),
                    /*preview_image_url=*/GURL(),
                    gfx::Size(10, 10),
                    /*full_url=*/GURL(),
                    gfx::Size(20, 20),
                    u"cat gif")},
        {"Category",
         QuickInsertCategoryResult(QuickInsertCategory::kEmojisGifs)},
        {"LocalFile", QuickInsertLocalFileResult(u"local", base::FilePath())},
        {"DriveFile", QuickInsertDriveFileResult(std::nullopt,
                                                 u"drive",
                                                 GURL(),
                                                 base::FilePath())},
    }),
    [](const testing::TestParamInfo<
        QuickInsertSearchResultsViewResultSelectionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash
