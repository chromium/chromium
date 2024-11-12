// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_zero_state_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/quick_insert/mock_quick_insert_asset_fetcher.h"
#include "ash/quick_insert/model/quick_insert_caps_lock_position.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/quick_insert/views/quick_insert_category_type.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_with_submenu_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/quick_insert/views/quick_insert_zero_state_view_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::IsSupersetOf;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::VariantWith;

constexpr int kQuickInsertWidth = 320;

template <class V, class Matcher>
auto AsView(Matcher matcher) {
  return ResultOf(
      "AsViewClass",
      [](views::View* view) { return views::AsViewClass<V>(view); },
      Pointee(matcher));
}

constexpr base::span<const QuickInsertCategory> kAllCategories = {
    (QuickInsertCategory[]){
        QuickInsertCategory::kEditorWrite,
        QuickInsertCategory::kEditorRewrite,
        QuickInsertCategory::kLobsterWithNoSelectedText,
        QuickInsertCategory::kLobsterWithSelectedText,
        QuickInsertCategory::kLinks,
        QuickInsertCategory::kEmojisGifs,
        QuickInsertCategory::kClipboard,
        QuickInsertCategory::kDriveFiles,
        QuickInsertCategory::kLocalFiles,
        QuickInsertCategory::kDatesTimes,
        QuickInsertCategory::kUnitsMaths,
    }};

class MockZeroStateViewDelegate : public PickerZeroStateViewDelegate {
 public:
  MOCK_METHOD(void, SelectZeroStateCategory, (QuickInsertCategory), (override));
  MOCK_METHOD(void,
              SelectZeroStateResult,
              (const QuickInsertSearchResult&),
              (override));
  MOCK_METHOD(void,
              GetZeroStateSuggestedResults,
              (SuggestedResultsCallback),
              (override));
  MOCK_METHOD(void, RequestPseudoFocus, (views::View*), (override));
  MOCK_METHOD(QuickInsertActionType,
              GetActionForResult,
              (const QuickInsertSearchResult& result),
              (override));
  MOCK_METHOD(void, OnZeroStateViewHeightChanged, (), (override));
  MOCK_METHOD(PickerCapsLockPosition, GetCapsLockPosition, (), (override));
  MOCK_METHOD(void, SetCapsLockDisplayed, (bool), (override));
};

class QuickInsertZeroStateViewTest : public views::ViewsTestBase {
 public:
  QuickInsertZeroStateViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  MockPickerAssetFetcher asset_fetcher_;
  PickerSubmenuController submenu_controller_;
  PickerPreviewBubbleController preview_controller_;

 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(QuickInsertZeroStateViewTest, CreatesCategorySections) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateView view(&mock_delegate, kAllCategories, kQuickInsertWidth,
                           &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(view.category_section_views_for_testing(),
              ElementsAre(Key(QuickInsertCategoryType::kEditorWrite),
                          Key(QuickInsertCategoryType::kGeneral),
                          Key(QuickInsertCategoryType::kMore)));
}

TEST_F(QuickInsertZeroStateViewTest, LeftClickSelectsCategory) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  MockZeroStateViewDelegate mock_delegate;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate,
      std::vector<QuickInsertCategory>{QuickInsertCategory::kEmojisGifs},
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();
  ASSERT_THAT(view->category_section_views_for_testing(),
              Contains(Key(QuickInsertCategoryType::kGeneral)));
  ASSERT_THAT(view->category_section_views_for_testing()
                  .find(QuickInsertCategoryType::kGeneral)
                  ->second->item_views_for_testing(),
              Not(IsEmpty()));

  EXPECT_CALL(mock_delegate,
              SelectZeroStateCategory(QuickInsertCategory::kEmojisGifs))
      .Times(1);

  QuickInsertItemView* category_view =
      view->category_section_views_for_testing()
          .find(QuickInsertCategoryType::kGeneral)
          ->second->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_view);
  LeftClickOn(*category_view);
}

TEST_F(QuickInsertZeroStateViewTest, ShowsSuggestedResults) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({QuickInsertDriveFileResult(
                /*id=*/std::nullopt,
                /*title=*/u"test drive file",
                /*url=*/GURL(), base::FilePath())});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const QuickInsertSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kQuickInsertWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(
      mock_delegate,
      SelectZeroStateResult(VariantWith<ash::QuickInsertDriveFileResult>(
          Field("title", &ash::QuickInsertDriveFileResult::title,
                u"test drive file"))))
      .Times(1);

  ASSERT_THAT(
      view->primary_section_view_for_testing()->item_views_for_testing(),
      Not(IsEmpty()));
  QuickInsertItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(QuickInsertZeroStateViewTest,
       ShowsSuggestedLocalFileResultsInRowFormat) {
  base::test::ScopedFeatureList feature_list(features::kPickerGrid);
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({QuickInsertLocalFileResult(u"a", {}),
                                     QuickInsertLocalFileResult(u"b", {}),
                                     QuickInsertLocalFileResult(u"c", {})});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kQuickInsertWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(
      mock_delegate,
      SelectZeroStateResult(VariantWith<ash::QuickInsertLocalFileResult>(_)))
      .Times(1);

  ASSERT_THAT(
      view->primary_section_view_for_testing()->item_views_for_testing(),
      IsSupersetOf({
          AsView<PickerImageItemView>(
              Property(&QuickInsertListItemView::GetAccessibleName, u"a")),
          AsView<PickerImageItemView>(
              Property(&QuickInsertListItemView::GetAccessibleName, u"b")),
          AsView<PickerImageItemView>(
              Property(&QuickInsertListItemView::GetAccessibleName, u"c")),
      }));
  QuickInsertItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[0];
  ASSERT_TRUE(views::IsViewClass<PickerImageItemView>(item_view));
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(QuickInsertZeroStateViewTest, ShowsMoreItemsButtonForLocalFiles) {
  base::test::ScopedFeatureList feature_list(features::kPickerGrid);
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({QuickInsertLocalFileResult({}, {})});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kQuickInsertWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateCategory(QuickInsertCategory::kLocalFiles))
      .Times(1);

  views::View* more_items_button = view->primary_section_view_for_testing()
                                       ->GetImageRowMoreItemsButtonForTesting();
  ASSERT_TRUE(more_items_button);
  ViewDrawnWaiter().Wait(more_items_button);
  LeftClickOn(*more_items_button);
}

TEST_F(QuickInsertZeroStateViewTest,
       DisplayingCapsLockResultSetsCapsLockDisplayed) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {QuickInsertDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath()),
                 QuickInsertCapsLockResult(
                     /*enabled=*/true,
                     QuickInsertCapsLockResult::Shortcut::kAltSearch)});
          });
  EXPECT_CALL(mock_delegate, SetCapsLockDisplayed(true)).Times(1);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kQuickInsertWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();
}

TEST_F(QuickInsertZeroStateViewTest,
       PutsCapsLockAtTheEndOfSuggestedResultsForMiddleCase) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetCapsLockPosition)
      .WillOnce(Return(PickerCapsLockPosition::kMiddle));
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {QuickInsertCapsLockResult(
                     /*enabled=*/true,
                     QuickInsertCapsLockResult::Shortcut::kAltSearch),
                 QuickInsertDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath())});
          });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const QuickInsertSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate,
      std::vector<QuickInsertCategory>{QuickInsertCategory::kDatesTimes,
                                       QuickInsertCategory::kUnitsMaths},
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();
  task_environment()->AdvanceClock(base::Seconds(1));
  task_environment()->RunUntilIdle();

  EXPECT_CALL(
      mock_delegate,
      SelectZeroStateResult(VariantWith<ash::QuickInsertCapsLockResult>(
          Field("enabled", &ash::QuickInsertCapsLockResult::enabled, true))))
      .Times(1);

  QuickInsertItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[1];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(QuickInsertZeroStateViewTest, PutsCapsLockInMoreCategoryForBottomCase) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetCapsLockPosition)
      .WillOnce(Return(PickerCapsLockPosition::kBottom));
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {QuickInsertCapsLockResult(
                     /*enabled=*/true,
                     QuickInsertCapsLockResult::Shortcut::kAltSearch),
                 QuickInsertDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath())});
          });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const QuickInsertSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate,
      std::vector<QuickInsertCategory>{QuickInsertCategory::kDatesTimes,
                                       QuickInsertCategory::kUnitsMaths},
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();

  EXPECT_CALL(
      mock_delegate,
      SelectZeroStateResult(VariantWith<ash::QuickInsertCapsLockResult>(
          Field("enabled", &ash::QuickInsertCapsLockResult::enabled, true))))
      .Times(1);

  QuickInsertItemView* item_view = view->category_section_views_for_testing()
                                       .find(QuickInsertCategoryType::kMore)
                                       ->second->item_views_for_testing()[2];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(QuickInsertZeroStateViewTest,
       DoesntShowEditorRewriteCategoryForEmptySuggestions) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({});
          });
  PickerZeroStateView view(
      &mock_delegate, base::span_from_ref(QuickInsertCategory::kEditorRewrite),
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_);

  EXPECT_THAT(view.primary_section_view_for_testing(), IsNull());
}

TEST_F(QuickInsertZeroStateViewTest,
       ShowsEditorSuggestionsAsItemsWithoutSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({
                QuickInsertEditorResult(
                    QuickInsertEditorResult::Mode::kRewrite,
                    /*display_name=*/u"a",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kUnknown,
                    "query_a"),
                QuickInsertEditorResult(
                    QuickInsertEditorResult::Mode::kRewrite,
                    /*display_name=*/u"b",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kUnknown,
                    "query_b"),
            });
          });
  PickerZeroStateView view(
      &mock_delegate, base::span_from_ref(QuickInsertCategory::kEditorRewrite),
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_);

  EXPECT_THAT(
      view.primary_section_view_for_testing(),
      Pointee(AllOf(
          Property("GetVisible", &views::View::GetVisible, true),
          Property("item_views_for_testing",
                   &QuickInsertSectionView::item_views_for_testing,
                   ElementsAre(
                       AsView<QuickInsertListItemView>(Property(
                           &QuickInsertListItemView::GetPrimaryTextForTesting,
                           u"a")),
                       AsView<QuickInsertListItemView>(Property(
                           &QuickInsertListItemView::GetPrimaryTextForTesting,
                           u"b")))))));
}

TEST_F(QuickInsertZeroStateViewTest, ShowsEditorSuggestionsBehindSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({
                QuickInsertEditorResult(
                    QuickInsertEditorResult::Mode::kRewrite,
                    /*display_name=*/u"a",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kShorten,
                    "shorten"),
                QuickInsertEditorResult(
                    QuickInsertEditorResult::Mode::kRewrite,
                    /*display_name=*/u"b",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kEmojify,
                    "emojify"),
            });
          });
  PickerZeroStateView view(
      &mock_delegate, base::span_from_ref(QuickInsertCategory::kEditorRewrite),
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_);

  EXPECT_THAT(
      view.primary_section_view_for_testing(),
      Pointee(AllOf(
          Property("GetVisible", &views::View::GetVisible, true),
          Property(
              "item_views_for_testing",
              &QuickInsertSectionView::item_views_for_testing,
              ElementsAre(AsView<PickerItemWithSubmenuView>(Property(
                              &PickerItemWithSubmenuView::GetTextForTesting,
                              l10n_util::GetStringUTF16(
                                  IDS_PICKER_CHANGE_LENGTH_MENU_LABEL))),
                          AsView<PickerItemWithSubmenuView>(Property(
                              &PickerItemWithSubmenuView::GetTextForTesting,
                              l10n_util::GetStringUTF16(
                                  IDS_PICKER_CHANGE_TONE_MENU_LABEL))))))));
}

TEST_F(QuickInsertZeroStateViewTest,
       DoesntShowLobsterWithTextSelectionCategoryForEmptySuggestions) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({});
          });
  PickerZeroStateView view(
      &mock_delegate,
      base::span_from_ref(QuickInsertCategory::kLobsterWithSelectedText),
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_);

  EXPECT_THAT(view.category_section_views_for_testing(), IsEmpty());
}

TEST_F(QuickInsertZeroStateViewTest, ShowLobsterCategoryAsListItem) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({QuickInsertLobsterResult(
                QuickInsertLobsterResult::Mode::kWithSelection,
                /*display_name=*/u"lobster")});
          });
  PickerZeroStateView view(
      &mock_delegate,
      base::span_from_ref(QuickInsertCategory::kLobsterWithSelectedText),
      kQuickInsertWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_);

  EXPECT_THAT(
      view.category_section_views_for_testing(),
      ElementsAre(Pair(
          QuickInsertCategoryType::kLobster,
          Pointee(AllOf(
              Property("GetVisible", &views::View::GetVisible, true),
              Property("item_views_for_testing",
                       &QuickInsertSectionView::item_views_for_testing,
                       ElementsAre(AsView<QuickInsertListItemView>(Property(
                           &QuickInsertListItemView::GetPrimaryTextForTesting,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                           l10n_util::GetStringUTF16(
                               IDS_PICKER_LOBSTER_SELECTION_LABEL)
#else
                           u""
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
                               )))))))));
}

TEST_F(QuickInsertZeroStateViewTest, ShowsCaseTransformationBehindSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({
                QuickInsertCaseTransformResult(
                    QuickInsertCaseTransformResult::kUpperCase),
                QuickInsertCaseTransformResult(
                    QuickInsertCaseTransformResult::kLowerCase),
                QuickInsertCaseTransformResult(
                    QuickInsertCaseTransformResult::kTitleCase),
            });
          });
  PickerZeroStateView view(&mock_delegate, {}, kQuickInsertWidth,
                           &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(
      view.category_section_views_for_testing(),
      ElementsAre(Pair(
          QuickInsertCategoryType::kCaseTransformations,
          Pointee(AllOf(
              Property("GetVisible", &views::View::GetVisible, true),
              Property(
                  "item_views_for_testing",
                  &QuickInsertSectionView::item_views_for_testing,
                  ElementsAre(AsView<PickerItemWithSubmenuView>(Property(
                      &PickerItemWithSubmenuView::GetTextForTesting,
                      l10n_util::GetStringUTF16(
                          IDS_PICKER_CHANGE_CAPITALIZATION_MENU_LABEL))))))))));
}

TEST_F(QuickInsertZeroStateViewTest,
       RequestsPseudoFocusAfterGettingSuggestedItems) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateViewDelegate::SuggestedResultsCallback
      suggested_results_callback;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [&](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            suggested_results_callback = std::move(callback);
          });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kQuickInsertWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate, RequestPseudoFocus(_));

  suggested_results_callback.Run({QuickInsertDriveFileResult(
      /*id=*/std::nullopt,
      /*title=*/u"test drive file",
      /*url=*/GURL(), base::FilePath())});
}

}  // namespace
}  // namespace ash
