// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/mock_picker_asset_fetcher.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
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
    PickerCategory::kLinks,
    PickerCategory::kExpressions,
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
              GetSuggestedZeroStateEditorResults,
              (SuggestedEditorResultsCallback),
              (override));
  MOCK_METHOD(void,
              GetZeroStateRecentResults,
              (PickerCategory, SearchResultsCallback),
              (override));
  MOCK_METHOD(void, NotifyPseudoFocusChanged, (views::View*), (override));
  MOCK_METHOD(PickerActionType,
              GetActionForResult,
              (const PickerSearchResult& result),
              (override));
};

class PickerZeroStateViewTest : public views::ViewsTestBase {
 protected:
  MockPickerAssetFetcher asset_fetcher_;

 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerZeroStateViewTest, CreatesCategorySections) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateView view(&mock_delegate, kAllCategories, {}, kPickerWidth,
                           &asset_fetcher_);

  EXPECT_THAT(view.category_section_views_for_testing(),
              ElementsAre(Key(PickerCategoryType::kEditorWrite),
                          Key(PickerCategoryType::kEditorRewrite),
                          Key(PickerCategoryType::kGeneral),
                          Key(PickerCategoryType::kCalculations)));
  EXPECT_THAT(view.PrimarySectionForTesting().item_views_for_testing(),
              IsEmpty());
}

TEST_F(PickerZeroStateViewTest, LeftClickSelectsCategory) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  MockZeroStateViewDelegate mock_delegate;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, std::vector<PickerCategory>{PickerCategory::kExpressions},
      std::vector<PickerCategory>{}, kPickerWidth, &asset_fetcher_));
  widget->Show();
  ASSERT_THAT(view->category_section_views_for_testing(),
              Contains(Key(PickerCategoryType::kGeneral)));
  ASSERT_THAT(view->category_section_views_for_testing()
                  .find(PickerCategoryType::kGeneral)
                  ->second->item_views_for_testing(),
              Not(IsEmpty()));

  EXPECT_CALL(mock_delegate,
              SelectZeroStateCategory(PickerCategory::kExpressions))
      .Times(1);

  PickerItemView* category_view = view->category_section_views_for_testing()
                                      .find(PickerCategoryType::kGeneral)
                                      ->second->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_view);
  LeftClickOn(*category_view);
}

TEST_F(PickerZeroStateViewTest, ShowsRecentItems) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetZeroStateRecentResults(PickerCategory::kDriveFiles, _))
      .WillOnce([](PickerCategory category,
                   MockZeroStateViewDelegate::SearchResultsCallback callback) {
        std::move(callback).Run({PickerSearchResult::DriveFile(
            /*title=*/u"test drive file",
            /*url=*/GURL(), base::FilePath())});
      });

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories,
      std::vector<PickerCategory>{PickerCategory::kDriveFiles}, kPickerWidth,
      &asset_fetcher_));
  widget->Show();

  EXPECT_CALL(mock_delegate,
              SelectZeroStateResult(Property(
                  "data", &ash::PickerSearchResult::data,
                  VariantWith<ash::PickerSearchResult::DriveFileData>(Field(
                      "title", &ash::PickerSearchResult::DriveFileData::title,
                      u"test drive file")))))
      .Times(1);

  ASSERT_THAT(view->PrimarySectionForTesting().item_views_for_testing(),
              Not(IsEmpty()));
  PickerItemView* item_view =
      view->PrimarySectionForTesting().item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);
}

TEST_F(PickerZeroStateViewTest,
       DoesntShowEditorRewriteCategoryForEmptySuggestions) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetSuggestedZeroStateEditorResults)
      .WillOnce([](MockZeroStateViewDelegate::SuggestedEditorResultsCallback
                       callback) { std::move(callback).Run({}); });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kEditorRewrite}},
                           {}, kPickerWidth, &asset_fetcher_);

  EXPECT_THAT(
      view.category_section_views_for_testing(),
      ElementsAre(Pair(
          PickerCategoryType::kEditorRewrite,
          Pointee(Property("GetVisible", &views::View::GetVisible, false)))));
}

TEST_F(PickerZeroStateViewTest, ShowsEditorSuggestionsAsItems) {
  MockZeroStateViewDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetSuggestedZeroStateEditorResults)
      .WillOnce([](MockZeroStateViewDelegate::SuggestedEditorResultsCallback
                       callback) {
        std::move(callback).Run({
            PickerSearchResult::Editor(
                PickerSearchResult::EditorData::Mode::kRewrite,
                /*display_name=*/u"a",
                /*category=(*/ std::nullopt, "query_a",
                /*freeform_text=*/std::nullopt),
            PickerSearchResult::Editor(
                PickerSearchResult::EditorData::Mode::kRewrite,
                /*display_name=*/u"b",
                /*category=(*/ std::nullopt, "query_b",
                /*freeform_text=*/std::nullopt),
        });
      });
  PickerZeroStateView view(&mock_delegate, {{PickerCategory::kEditorRewrite}},
                           {}, kPickerWidth, &asset_fetcher_);

  EXPECT_THAT(
      view.category_section_views_for_testing(),
      ElementsAre(Pair(
          PickerCategoryType::kEditorRewrite,
          Pointee(AllOf(
              Property("GetVisible", &views::View::GetVisible, true),
              Property(
                  "item_views_for_testing",
                  &PickerSectionView::item_views_for_testing,
                  ElementsAre(
                      AsView<PickerListItemView>(Property(
                          &PickerListItemView::GetPrimaryTextForTesting, u"a")),
                      AsView<PickerListItemView>(Property(
                          &PickerListItemView::GetPrimaryTextForTesting,
                          u"b")))))))));
}

TEST_F(PickerZeroStateViewTest, UpdatesPseudoFocusToTopRecentItem) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateViewDelegate::SearchResultsCallback recent_results_callback;
  EXPECT_CALL(mock_delegate,
              GetZeroStateRecentResults(PickerCategory::kDriveFiles, _))
      .WillOnce([&](PickerCategory category,
                    MockZeroStateViewDelegate::SearchResultsCallback callback) {
        recent_results_callback = std::move(callback);
      });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories,
      std::vector<PickerCategory>{PickerCategory::kDriveFiles}, kPickerWidth,
      &asset_fetcher_));
  widget->Show();

  view->GainPseudoFocus(
      PickerPseudoFocusHandler::PseudoFocusDirection::kForward);
  recent_results_callback.Run({PickerSearchResult::DriveFile(
      /*title=*/u"test drive file",
      /*url=*/GURL(), base::FilePath())});

  EXPECT_CALL(mock_delegate,
              SelectZeroStateResult(Property(
                  "data", &ash::PickerSearchResult::data,
                  VariantWith<ash::PickerSearchResult::DriveFileData>(Field(
                      "title", &ash::PickerSearchResult::DriveFileData::title,
                      u"test drive file")))))
      .Times(1);
  EXPECT_TRUE(view->DoPseudoFocusedAction());
}

TEST_F(PickerZeroStateViewTest, NoPseudoFocusActionAfterLosingPseudoFocus) {
  MockZeroStateViewDelegate mock_delegate;
  PickerZeroStateViewDelegate::SearchResultsCallback recent_results_callback;
  EXPECT_CALL(mock_delegate,
              GetZeroStateRecentResults(PickerCategory::kDriveFiles, _))
      .WillOnce([&](PickerCategory category,
                    MockZeroStateViewDelegate::SearchResultsCallback callback) {
        recent_results_callback = std::move(callback);
      });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetFullscreen(true);
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      &mock_delegate, kAllCategories,
      std::vector<PickerCategory>{PickerCategory::kDriveFiles}, kPickerWidth,
      &asset_fetcher_));
  widget->Show();

  view->GainPseudoFocus(
      PickerPseudoFocusHandler::PseudoFocusDirection::kForward);
  view->LosePseudoFocus();
  recent_results_callback.Run({PickerSearchResult::DriveFile(
      /*title=*/u"test drive file",
      /*url=*/GURL(), base::FilePath())});

  EXPECT_FALSE(view->DoPseudoFocusedAction());
}

}  // namespace
}  // namespace ash
