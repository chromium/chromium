// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/model/picker_caps_lock_position.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/test/view_drawn_waiter.h"
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

constexpr int kPickerWidth = 320;

template <class V, class Matcher>
auto AsView(Matcher matcher) {
  return ResultOf(
      "AsViewClass",
      [](views::View* view) { return views::AsViewClass<V>(view); },
      Pointee(matcher));
}

constexpr base::span<const PickerCategory> kAllCategories = {(PickerCategory[]){
    PickerCategory::kEditorWrite,
    PickerCategory::kEditorRewrite,
    PickerCategory::kLobster,
    PickerCategory::kLinks,
    PickerCategory::kEmojisGifs,
    PickerCategory::kClipboard,
    PickerCategory::kDriveFiles,
    PickerCategory::kLocalFiles,
    PickerCategory::kDatesTimes,
    PickerCategory::kUnitsMaths,
}};

class MockZeroStateViewDelegate : public PickerZeroStateViewDelegate {
 public:
  MOCK_METHOD(void, SelectZeroStateCategory, (PickerCategory), (override));
  MOCK_METHOD(void,
              SelectZeroStateResult,
              (const PickerSearchResult&),
              (override));
  MOCK_METHOD(void,
              GetZeroStateSuggestedResults,
              (SuggestedResultsCallback),
              (override));
  MOCK_METHOD(void, RequestPseudoFocus, (views::View*), (override));
  MOCK_METHOD(PickerActionType,
              GetActionForResult,
              (const PickerSearchResult& result),
              (override));
  MOCK_METHOD(void, OnZeroStateViewHeightChanged, (), (override));
  MOCK_METHOD(PickerCapsLockPosition, GetCapsLockPosition, (), (override));
  MOCK_METHOD(void, SetCapsLockDisplayed, (bool), (override));
};

class PickerZeroStateViewTest : public views::ViewsTestBase {
 public:
  PickerZeroStateViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  MockPickerAssetFetcher asset_fetcher_;
  PickerSubmenuController submenu_controller_;
  PickerPreviewBubbleController preview_controller_;

 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerZeroStateViewTest, CreatesCategorySections) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateView view(&mock_delegate, kAllCategories, kPickerWidth,
                           &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(view.category_section_views_for_testing(),
              ElementsAre(Key(PickerCategoryType::kEditorWrite),
                          Key(PickerCategoryType::kGeneral),
                          Key(PickerCategoryType::kMore)));
}

TEST_F(PickerZeroStateViewTest, LeftClickSelectsCategory) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  MockZeroStateViewDelegate mock_delegate;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, std::vector<PickerCategory>{PickerCategory::kEmojisGifs},
      kPickerWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();
  ASSERT_THAT(view->category_section_views_for_testing(),
              Contains(Key(PickerCategoryType::kGeneral)));
  ASSERT_THAT(view->category_section_views_for_testing()
                  .find(PickerCategoryType::kGeneral)
                  ->second->item_views_for_testing(),
              Not(IsEmpty()));

  EXPECT_CALL(mock_delegate,
              SelectZeroStateCategory(PickerCategory::kEmojisGifs))
      .Times(1);

  PickerItemView* category_view = view->category_section_views_for_testing()
                                      .find(PickerCategoryType::kGeneral)
                                      ->second->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_view);
  LeftClickOn(*category_view);
}

TEST_F(PickerZeroStateViewTest, ShowsSuggestedResults) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({PickerDriveFileResult(
                /*id=*/std::nullopt,
                /*title=*/u"test drive file",
                /*url=*/GURL(), base::FilePath())});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kPickerWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(
      mock_delegate,
      SelectZeroStateResult(VariantWith<ash::PickerDriveFileResult>(Field(
          "title", &ash::PickerDriveFileResult::title, u"test drive file"))))
      .Times(1);

  ASSERT_THAT(
      view->primary_section_view_for_testing()->item_views_for_testing(),
      Not(IsEmpty()));
  PickerItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(PickerZeroStateViewTest, ShowsSuggestedLocalFileResultsInRowFormat) {
  base::test::ScopedFeatureList feature_list(features::kPickerGrid);
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({PickerLocalFileResult(u"a", {}),
                                     PickerLocalFileResult(u"b", {}),
                                     PickerLocalFileResult(u"c", {})});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kPickerWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateResult(VariantWith<ash::PickerLocalFileResult>(_)))
      .Times(1);

  ASSERT_THAT(
      view->primary_section_view_for_testing()->item_views_for_testing(),
      IsSupersetOf({
          AsView<PickerImageItemView>(
              Property(&PickerListItemView::GetAccessibleName, u"a")),
          AsView<PickerImageItemView>(
              Property(&PickerListItemView::GetAccessibleName, u"b")),
          AsView<PickerImageItemView>(
              Property(&PickerListItemView::GetAccessibleName, u"c")),
      }));
  PickerItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[0];
  ASSERT_TRUE(views::IsViewClass<PickerImageItemView>(item_view));
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(PickerZeroStateViewTest, ShowsMoreItemsButtonForLocalFiles) {
  base::test::ScopedFeatureList feature_list(features::kPickerGrid);
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({PickerLocalFileResult({}, {})});
          });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kPickerWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateCategory(PickerCategory::kLocalFiles))
      .Times(1);

  views::View* more_items_button = view->primary_section_view_for_testing()
                                       ->GetImageRowMoreItemsButtonForTesting();
  ASSERT_TRUE(more_items_button);
  ViewDrawnWaiter().Wait(more_items_button);
  LeftClickOn(*more_items_button);
}

TEST_F(PickerZeroStateViewTest, DisplayingCapsLockResultSetsCapsLockDisplayed) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {PickerDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath()),
                 PickerCapsLockResult(
                     /*enabled=*/true,
                     PickerCapsLockResult::Shortcut::kAltSearch)});
          });
  EXPECT_CALL(mock_delegate, SetCapsLockDisplayed(true)).Times(1);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories, kPickerWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();
}

TEST_F(PickerZeroStateViewTest,
       PutsCapsLockAtTheEndOfSuggestedResultsForMiddleCase) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetCapsLockPosition)
      .WillOnce(Return(PickerCapsLockPosition::kMiddle));
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {PickerCapsLockResult(
                     /*enabled=*/true,
                     PickerCapsLockResult::Shortcut::kAltSearch),
                 PickerDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath())});
          });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate,
      std::vector<PickerCategory>{PickerCategory::kDatesTimes,
                                  PickerCategory::kUnitsMaths},
      kPickerWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();
  task_environment()->AdvanceClock(base::Seconds(1));
  task_environment()->RunUntilIdle();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateResult(VariantWith<ash::PickerCapsLockResult>(
                  Field("enabled", &ash::PickerCapsLockResult::enabled, true))))
      .Times(1);

  PickerItemView* item_view =
      view->primary_section_view_for_testing()->item_views_for_testing()[1];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(PickerZeroStateViewTest, PutsCapsLockInMoreCategoryForBottomCase) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetCapsLockPosition)
      .WillOnce(Return(PickerCapsLockPosition::kBottom));
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults(_))
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run(
                {PickerCapsLockResult(
                     /*enabled=*/true,
                     PickerCapsLockResult::Shortcut::kAltSearch),
                 PickerDriveFileResult(
                     /*id=*/std::nullopt,
                     /*title=*/u"test drive file",
                     /*url=*/GURL(), base::FilePath())});
          });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate,
      std::vector<PickerCategory>{PickerCategory::kDatesTimes,
                                  PickerCategory::kUnitsMaths},
      kPickerWidth, &asset_fetcher_, &submenu_controller_,
      &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateResult(VariantWith<ash::PickerCapsLockResult>(
                  Field("enabled", &ash::PickerCapsLockResult::enabled, true))))
      .Times(1);

  PickerItemView* item_view = view->category_section_views_for_testing()
                                  .find(PickerCategoryType::kMore)
                                  ->second->item_views_for_testing()[2];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(PickerZeroStateViewTest,
       DoesntShowEditorRewriteCategoryForEmptySuggestions) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({});
          });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kEditorRewrite}},
                           kPickerWidth, &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(view.primary_section_view_for_testing(), IsNull());
}

TEST_F(PickerZeroStateViewTest, ShowsEditorSuggestionsAsItemsWithoutSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({
                PickerEditorResult(
                    PickerEditorResult::Mode::kRewrite,
                    /*display_name=*/u"a",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kUnknown,
                    "query_a"),
                PickerEditorResult(
                    PickerEditorResult::Mode::kRewrite,
                    /*display_name=*/u"b",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kUnknown,
                    "query_b"),
            });
          });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kEditorRewrite}},
                           kPickerWidth, &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(
      view.primary_section_view_for_testing(),
      Pointee(AllOf(
          Property("GetVisible", &views::View::GetVisible, true),
          Property(
              "item_views_for_testing",
              &PickerSectionView::item_views_for_testing,
              ElementsAre(
                  AsView<PickerListItemView>(Property(
                      &PickerListItemView::GetPrimaryTextForTesting, u"a")),
                  AsView<PickerListItemView>(
                      Property(&PickerListItemView::GetPrimaryTextForTesting,
                               u"b")))))));
}

TEST_F(PickerZeroStateViewTest, ShowsEditorSuggestionsBehindSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({
                PickerEditorResult(
                    PickerEditorResult::Mode::kRewrite,
                    /*display_name=*/u"a",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kShorten,
                    "shorten"),
                PickerEditorResult(
                    PickerEditorResult::Mode::kRewrite,
                    /*display_name=*/u"b",
                    /*category=*/
                    chromeos::editor_menu::PresetQueryCategory::kEmojify,
                    "emojify"),
            });
          });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kEditorRewrite}},
                           kPickerWidth, &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(
      view.primary_section_view_for_testing(),
      Pointee(AllOf(
          Property("GetVisible", &views::View::GetVisible, true),
          Property(
              "item_views_for_testing",
              &PickerSectionView::item_views_for_testing,
              ElementsAre(AsView<PickerItemWithSubmenuView>(Property(
                              &PickerItemWithSubmenuView::GetTextForTesting,
                              l10n_util::GetStringUTF16(
                                  IDS_PICKER_CHANGE_LENGTH_MENU_LABEL))),
                          AsView<PickerItemWithSubmenuView>(Property(
                              &PickerItemWithSubmenuView::GetTextForTesting,
                              l10n_util::GetStringUTF16(
                                  IDS_PICKER_CHANGE_TONE_MENU_LABEL))))))));
}

TEST_F(PickerZeroStateViewTest, DoesntShowLobsterCategoryForEmptySuggestions) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({});
          });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kLobster}},
                           kPickerWidth, &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(view.primary_section_view_for_testing(), IsNull());
}

TEST_F(PickerZeroStateViewTest, ShowLobsterCategoryAsItemWithSubMenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce(
          [](MockZeroStateViewDelegate::SuggestedResultsCallback callback) {
            std::move(callback).Run({PickerLobsterResult(
                /*display_name=*/u"lobster")});
          });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kLobster}},
                           kPickerWidth, &asset_fetcher_, &submenu_controller_,
                           &preview_controller_);

  EXPECT_THAT(
      view.primary_section_view_for_testing(),
      Pointee(AllOf(
          Property("GetVisible", &views::View::GetVisible, true),
          Property(
              "item_views_for_testing",
              &PickerSectionView::item_views_for_testing,
              ElementsAre(AsView<PickerListItemView>(Property(
                  &PickerListItemView::GetPrimaryTextForTesting,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                  l10n_util::GetStringUTF16(IDS_PICKER_LOBSTER_SELECTION_LABEL)
#else
                  u""
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
                      )))))));
}

TEST_F(PickerZeroStateViewTest, ShowsCaseTransformationBehindSubmenu) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetZeroStateSuggestedResults)
      .WillOnce([](MockZeroStateViewDelegate::SuggestedResultsCallback
                       callback) {
        std::move(callback).Run({
            PickerCaseTransformResult(PickerCaseTransformResult::kUpperCase),
            PickerCaseTransformResult(PickerCaseTransformResult::kLowerCase),
            PickerCaseTransformResult(PickerCaseTransformResult::kTitleCase),
        });
      });
  PickerZeroStateView view(&mock_delegate, {}, kPickerWidth, &asset_fetcher_,
                           &submenu_controller_, &preview_controller_);

  EXPECT_THAT(
      view.category_section_views_for_testing(),
      ElementsAre(Pair(
          PickerCategoryType::kCaseTransformations,
          Pointee(AllOf(
              Property("GetVisible", &views::View::GetVisible, true),
              Property(
                  "item_views_for_testing",
                  &PickerSectionView::item_views_for_testing,
                  ElementsAre(AsView<PickerItemWithSubmenuView>(Property(
                      &PickerItemWithSubmenuView::GetTextForTesting,
                      l10n_util::GetStringUTF16(
                          IDS_PICKER_CHANGE_CAPITALIZATION_MENU_LABEL))))))))));
}

TEST_F(PickerZeroStateViewTest, RequestsPseudoFocusAfterGettingSuggestedItems) {
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
      &mock_delegate, kAllCategories, kPickerWidth, &asset_fetcher_,
      &submenu_controller_, &preview_controller_));
  widget->Show();

  EXPECT_CALL(mock_delegate, RequestPseudoFocus(_));

  suggested_results_callback.Run({PickerDriveFileResult(
      /*id=*/std::nullopt,
      /*title=*/u"test drive file",
      /*url=*/GURL(), base::FilePath())});
}

}  // namespace
}  // namespace ash
