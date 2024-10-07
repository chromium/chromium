// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <optional>
#include <string>

#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/mock_picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_skeleton_loader_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/picker/views/picker_traversable_item_container.h"
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

TEST_F(PickerSearchResultsViewTest, CreatesResultsSections) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerTextResult(u"Result A"), PickerTextResult(u"Result B")}},
      /*has_more_results=*/false));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerLocalFileResult(u"Result C", base::FilePath())}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(2)),
                          Pointee(MatchesResultSection(
                              PickerSectionType::kLocalFiles, 1))));
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResultsClearsView) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard, {{PickerTextResult(u"Result")}},
      /*has_more_results=*/false));

  view.ClearSearchResults();

  EXPECT_THAT(view.section_list_view_for_testing()->children(), IsEmpty());
}

TEST_F(PickerSearchResultsViewTest, EmptySearchResultsShowsThrobber) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard, {{PickerTextResult(u"Result")}},
      /*has_more_results=*/false));

  view.ClearSearchResults();

  EXPECT_TRUE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithGif) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerGifResult(
          /*preview_url=*/GURL(), /*preview_image_url=*/GURL(), gfx::Size(),
          /*full_url=*/GURL(), gfx::Size(),
          /*content_description=*/u"")}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(1))));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithCategories) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerCategoryResult(PickerCategory::kEmojisGifs)}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(1));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesTitlelessSection(1))));
}

TEST_F(PickerSearchResultsViewTest, CreatesResultsSectionWithLocalFiles) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerLocalFileResult(u"local", base::FilePath())}},
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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerDriveFileResult(/*id=*/std::nullopt, u"drive", GURL(),
                              base::FilePath())}},
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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles,
      {{PickerLocalFileResult(u"Result", base::FilePath())}},
      /*has_more_results=*/false));
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone, {{PickerTextResult(u"New Result")}},
      /*has_more_results=*/false));

  EXPECT_THAT(view.section_list_view_for_testing()->children(), SizeIs(2));
  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Pointee(MatchesResultSection(
                              PickerSectionType::kLocalFiles, 1)),
                          Pointee(MatchesTitlelessSection(1))));
}

TEST_F(PickerSearchResultsViewTest, GetsTopItem) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  EXPECT_CALL(mock_delegate, SelectSearchResult(VariantWith<PickerTextResult>(
                                 PickerTextResult(u"Result A"))));

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard,
      {{PickerTextResult(u"Result A"), PickerTextResult(u"Result B")}},
      /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetTopItem()));
}

TEST_F(PickerSearchResultsViewTest, GetsBottomItem) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  EXPECT_CALL(mock_delegate, SelectSearchResult(VariantWith<PickerTextResult>(
                                 PickerTextResult(u"Result B"))));

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kClipboard,
      {{PickerTextResult(u"Result A"), PickerTextResult(u"Result B")}},
      /*has_more_results=*/false));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(view.GetBottomItem()));
}

TEST_F(PickerSearchResultsViewTest, GetsItemAbove) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerCategoryResult(PickerCategory::kLinks),
        PickerCategoryResult(PickerCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<PickerCategoryResult>(
                  PickerCategoryResult(PickerCategory::kLinks))));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(
      view.GetItemAbove(view.GetBottomItem())));
}

TEST_F(PickerSearchResultsViewTest, GetsItemBelow) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone,
      {{PickerCategoryResult(PickerCategory::kLinks),
        PickerCategoryResult(PickerCategory::kClipboard)}},
      /*has_more_results=*/false));

  EXPECT_CALL(mock_delegate,
              SelectSearchResult(VariantWith<PickerCategoryResult>(
                  PickerCategoryResult(PickerCategory::kClipboard))));

  EXPECT_TRUE(
      DoPickerPseudoFocusedActionOnView(view.GetItemBelow(view.GetTopItem())));
}

TEST_F(PickerSearchResultsViewTest, ShowsSeeMoreLinkWhenThereAreMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller,
          &preview_controller));

  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/true));

  ASSERT_THAT(
      view->section_views_for_testing(),
      ElementsAre(Pointee(Property(
          "title_trailing_link_for_testing",
          &PickerSectionView::title_trailing_link_for_testing,
          Property(
              &views::View::GetAccessibleName,
              l10n_util::GetStringUTF16(
                  IDS_PICKER_SEE_MORE_LOCAL_FILES_BUTTON_ACCESSIBLE_NAME))))));
}

TEST_F(PickerSearchResultsViewTest,
       DoesNotShowSeeMoreLinkWhenThereAreNoMoreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller,
          &preview_controller));

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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller,
          &preview_controller));
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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  EXPECT_TRUE(view.SearchStopped(/*illustration=*/{}, u"no results"));

  EXPECT_FALSE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_TRUE(view.no_results_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_illustration_for_testing().GetVisible());
  EXPECT_TRUE(view.no_results_label_for_testing().GetVisible());
  EXPECT_EQ(view.no_results_label_for_testing().GetText(), u"no results");
}

TEST_F(PickerSearchResultsViewTest,
       SearchStoppedShowsNoResultsViewWithIllustration) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kLocalFiles, {}, /*has_more_results=*/true));
  EXPECT_FALSE(view.SearchStopped({}, u""));

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(PickerSearchResultsViewTest, SearchStoppedHidesLoaderView) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.ShowLoadingAnimation();
  ASSERT_TRUE(view.SearchStopped({}, u""));

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest, SearchStoppedHidesThrobber) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.ClearSearchResults();
  ASSERT_TRUE(view.SearchStopped({}, u""));

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest, ClearSearchResultsShowsSearchResults) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  ASSERT_TRUE(view.SearchStopped({}, u""));

  view.ClearSearchResults();

  EXPECT_TRUE(view.section_list_view_for_testing()->GetVisible());
  EXPECT_FALSE(view.no_results_view_for_testing()->GetVisible());
}

TEST_F(PickerSearchResultsViewTest, ShowLoadingShowsLoaderView) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.ShowLoadingAnimation();

  EXPECT_TRUE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerSearchResultsViewTest, ShowSkeletonLoaderHidesThrobber) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.ClearSearchResults();
  view.ShowLoadingAnimation();

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest, ShowLoadingAnimatesAfterDelay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  task_environment()->FastForwardBy(
      PickerSearchResultsView::kLoadingAnimationDelay);

  view.AppendSearchResults({PickerSearchResultsSection(
      PickerSectionType::kLinks, {PickerTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_FALSE(view.skeleton_loader_view_for_testing()
                   .layer()
                   ->GetAnimator()
                   ->is_animating());
}

TEST_F(PickerSearchResultsViewTest, AppendResultsDuringLoadingAppendsResults) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);
  view.ShowLoadingAnimation();

  view.AppendSearchResults({PickerSearchResultsSection(
      PickerSectionType::kLinks, {PickerTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.skeleton_loader_view_for_testing().GetVisible());
  EXPECT_THAT(view.section_views_for_testing(), SizeIs(1));
}

TEST_F(PickerSearchResultsViewTest, AppendResultsHidesThrobber) {
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  PickerSearchResultsView view(&mock_delegate, kPickerWidth, &asset_fetcher,
                               &submenu_controller, &preview_controller);

  view.AppendSearchResults({PickerSearchResultsSection(
      PickerSectionType::kLinks, {PickerTextResult(u"1")},
      /*has_more_results=*/false)});

  EXPECT_FALSE(view.throbber_container_for_testing().GetVisible());
}

TEST_F(PickerSearchResultsViewTest,
       StoppingSearchDoesNotAnnounceWhenThereAreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone, {}, /*has_more_results=*/false));
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(), u"");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 0);
}

TEST_F(PickerSearchResultsViewTest,
       StoppingSearchWithNoEmojisTriggersLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_TRUE(view->SearchStopped(/*illustration=*/{}, u""));

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(PickerSearchResultsViewTest,
       StoppingSearchWithEmojisTriggersLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));
  view->SetNumEmojiResultsForA11y(5);

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_TRUE(view->SearchStopped(/*illustration=*/{}, u""));

  EXPECT_EQ(view->GetAccessibleName(), u"5 emojis. No other results.");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(PickerSearchResultsViewTest,
       StoppingSearchConsecutivelyDoesNotTriggerLiveRegionChange) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SearchStopped(/*illustration=*/{}, u"");
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
}

TEST_F(PickerSearchResultsViewTest,
       StoppingSearchAfterClearingSearchAnnouncesWhenThereAreResults) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->SearchStopped(/*illustration=*/{}, u"");
  view->AppendSearchResults(PickerSearchResultsSection(
      PickerSectionType::kNone, {}, /*has_more_results=*/false));
  view->ClearSearchResults();
  view->SearchStopped(/*illustration=*/{}, u"");

  EXPECT_EQ(view->GetAccessibleName(),
            l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 2);
}

TEST_F(PickerSearchResultsViewTest, ClearingSearchResultsDoesNotAnnounce) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  MockPickerSearchResultsViewDelegate mock_delegate;
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, /*asset_fetcher=*/nullptr,
          /*submenu_controller=*/nullptr, /*preview_controller=*/nullptr));

  views::test::AXEventCounter counter(views::AXEventManager::Get());
  view->ClearSearchResults();

  EXPECT_EQ(view->GetAccessibleName(), u"");
  EXPECT_EQ(counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 0);
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
  MockPickerSearchResultsViewDelegate mock_delegate;
  MockPickerAssetFetcher asset_fetcher;
  PickerSubmenuController submenu_controller;
  PickerPreviewBubbleController preview_controller;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view =
      widget->SetContentsView(std::make_unique<PickerSearchResultsView>(
          &mock_delegate, kPickerWidth, &asset_fetcher, &submenu_controller,
          &preview_controller));
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
        {"Text", PickerTextResult(u"result")},
        {"Gif", PickerGifResult(
                    /*preview_url=*/GURL(),
                    /*preview_image_url=*/GURL(),
                    gfx::Size(10, 10),
                    /*full_url=*/GURL(),
                    gfx::Size(20, 20),
                    u"cat gif")},
        {"Category", PickerCategoryResult(PickerCategory::kEmojisGifs)},
        {"LocalFile", PickerLocalFileResult(u"local", base::FilePath())},
        {"DriveFile", PickerDriveFileResult(std::nullopt,
                                            u"drive",
                                            GURL(),
                                            base::FilePath())},
    }),
    [](const testing::TestParamInfo<
        PickerSearchResultsViewResultSelectionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash
