// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ime/composition_text.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace test {

class KeyPressCounterView : public ContentsView {
 public:
  explicit KeyPressCounterView(AppListView* app_list_view)
      : ContentsView(app_list_view), count_(0) {}
  ~KeyPressCounterView() override {}

  int GetCountAndReset() {
    int count = count_;
    count_ = 0;
    return count;
  }

 private:
  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& key_event) override {
    if (!::isalnum(static_cast<int>(key_event.key_code()))) {
      ++count_;
      return true;
    }
    return false;
  }
  int count_;

  DISALLOW_COPY_AND_ASSIGN(KeyPressCounterView);
};

class SearchBoxViewTest : public views::test::WidgetTest,
                          public SearchBoxViewDelegate {
 public:
  SearchBoxViewTest() = default;
  ~SearchBoxViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::test::WidgetTest::SetUp();

    app_list_view_ = new AppListView(&view_delegate_);
    app_list_view_->InitView(GetContext());

    widget_ = CreateTopLevelPlatformWidget();
    view_ =
        std::make_unique<SearchBoxView>(this, &view_delegate_, app_list_view());
    view_->Init(false /*is_tablet_mode*/);
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));
    counter_view_ = new KeyPressCounterView(app_list_view_);
    widget_->GetContentsView()->AddChildView(view());
    widget_->GetContentsView()->AddChildView(counter_view_);
    widget_->Show();
    counter_view_->Init(view_delegate_.GetModel());
    view()->set_contents_view(counter_view_);
  }

  void TearDown() override {
    view_.reset();
    app_list_view_->GetWidget()->Close();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_; }
  SearchBoxView* view() { return view_.get(); }
  AppListView* app_list_view() { return app_list_view_; }
  AppListTestViewDelegate* view_delegate() { return &view_delegate_; }

  void SetSearchEngineIsGoogle(bool is_google) {
    view_delegate_.SetSearchEngineIsGoogle(is_google);
  }

  void SetSearchBoxActive(bool active, ui::EventType type) {
    view()->SetSearchBoxActive(active, type);
  }

  int GetContentsViewKeyPressCountAndReset() {
    return counter_view_->GetCountAndReset();
  }

  void KeyPress(ui::KeyboardCode key_code, bool is_shift_down = false) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code,
                       is_shift_down ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    view()->search_box()->OnKeyEvent(&event);
    // Emulates the input method.
    if (::isalnum(static_cast<int>(key_code))) {
      base::char16 character = ::tolower(static_cast<int>(key_code));
      view()->search_box()->InsertText(base::string16(1, character));
    }
  }

  std::string GetLastQueryAndReset() {
    base::string16 query = last_query_;
    last_query_.clear();
    return base::UTF16ToUTF8(query);
  }

  int GetQueryChangedCountAndReset() {
    int result = query_changed_count_;
    query_changed_count_ = 0;
    return result;
  }

  // Creates a SearchResult with the given parameters.
  void CreateSearchResult(ash::SearchResultDisplayType display_type,
                          double display_score,
                          const base::string16& title,
                          const base::string16& details) {
    CreateSearchResultAt(results()->item_count(), display_type, display_score,
                         title, details);
  }

  // Creates a SearchResult with the given parameters at the given index in
  // the results list.
  void CreateSearchResultAt(size_t index,
                            ash::SearchResultDisplayType display_type,
                            double display_score,
                            const base::string16& title,
                            const base::string16& details) {
    auto search_result = std::make_unique<TestSearchResult>();
    search_result->set_result_id(base::NumberToString(++last_result_id_));
    search_result->set_display_type(display_type);
    search_result->set_display_score(display_score);
    search_result->set_title(title);
    search_result->set_details(details);
    results()->AddAt(index, std::move(search_result));
  }

  SearchModel::SearchResults* results() {
    return view_delegate_.GetSearchModel()->results();
  }

 private:
  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(SearchBoxViewBase* sender) override {
    ++query_changed_count_;
    last_query_ = sender->search_box()->GetText();
  }

  void AssistantButtonPressed() override {}
  void BackButtonPressed() override {}
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void SearchBoxFocusChanged(SearchBoxViewBase* sender) override {}

  AppListTestViewDelegate view_delegate_;
  views::Widget* widget_;
  AppListView* app_list_view_ = nullptr;
  std::unique_ptr<SearchBoxView> view_;
  KeyPressCounterView* counter_view_;
  base::string16 last_query_;
  int query_changed_count_ = 0;
  int last_result_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxViewTest);
};

// Tests that the close button is invisible by default.
TEST_F(SearchBoxViewTest, CloseButtonInvisibleByDefault) {
  EXPECT_FALSE(view()->close_button()->GetVisible());
}

// Tests that the close button becomes visible after typing in the search box.
TEST_F(SearchBoxViewTest, CloseButtonVisibleAfterTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_TRUE(view()->close_button()->GetVisible());
}

// Tests that the close button is still invisible after the search box is
// activated.
TEST_F(SearchBoxViewTest, CloseButtonInvisibleAfterSearchBoxActived) {
  SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // UI behavior is different with Zero State enabled.
  if (app_list_features::IsZeroStateSuggestionsEnabled())
    EXPECT_TRUE(view()->close_button()->GetVisible());
  else
    EXPECT_FALSE(view()->close_button()->GetVisible());
}

// Tests that the close button becomes invisible after close button is clicked.
TEST_F(SearchBoxViewTest, CloseButtonInvisibleAfterCloseButtonClicked) {
  KeyPress(ui::VKEY_A);
  view()->ButtonPressed(
      view()->close_button(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(view()->close_button()->GetVisible());
}

// Tests that the search box becomes empty after close button is clicked.
TEST_F(SearchBoxViewTest, SearchBoxEmptyAfterCloseButtonClicked) {
  KeyPress(ui::VKEY_A);
  view()->ButtonPressed(
      view()->close_button(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(view()->search_box()->GetText().empty());
}

// Tests that the search box is no longer active after close button is clicked.
TEST_F(SearchBoxViewTest, SearchBoxActiveAfterCloseButtonClicked) {
  KeyPress(ui::VKEY_A);
  view()->ButtonPressed(
      view()->close_button(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(view()->is_search_box_active());
}

// Tests that the search box is inactive by default.
TEST_F(SearchBoxViewTest, SearchBoxInactiveByDefault) {
  ASSERT_FALSE(view()->is_search_box_active());
}

// Tests that the black Google icon is used for an inactive Google search.
TEST_F(SearchBoxViewTest, SearchBoxInactiveSearchBoxGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kGoogleBlackIcon, kSearchBoxIconSize, kDefaultSearchboxColor);
  view()->ModelChanged();

  const gfx::ImageSkia actual_icon =
      view()->get_search_icon_for_test()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the colored Google icon is used for an active Google search.
TEST_F(SearchBoxViewTest, SearchBoxActiveSearchEngineGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kGoogleColorIcon, kSearchBoxIconSize, kDefaultSearchboxColor);
  view()->ModelChanged();

  const gfx::ImageSkia actual_icon =
      view()->get_search_icon_for_test()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an inactive non-Google search.
TEST_F(SearchBoxViewTest, SearchBoxInactiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, kSearchBoxIconSize, kDefaultSearchboxColor);
  view()->ModelChanged();

  const gfx::ImageSkia actual_icon =
      view()->get_search_icon_for_test()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an active non-Google search.
TEST_F(SearchBoxViewTest, SearchBoxActiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, kSearchBoxIconSize, kDefaultSearchboxColor);
  view()->ModelChanged();

  const gfx::ImageSkia actual_icon =
      view()->get_search_icon_for_test()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that traversing search results is disabled while results are being
// updated.
TEST_F(SearchBoxViewTest, ChangeSelectionWhileResultsAreChanging) {
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  view()->search_box()->SetText(base::ASCIIToUTF16("test"));
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7,
                     base::ASCIIToUTF16("tester"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("testing"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      view()->contents_view()->search_results_page_view();

  const SearchResultBaseView* selection =
      result_page_view->result_selection_controller()->selected_result();

  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Add a new result - the selection controller is updated asynchronously, so
  // the result is expected to remain the same until the loop is run.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1.,
                       base::ASCIIToUTF16("test"), base::string16());
  EXPECT_EQ(selection,
            result_page_view->result_selection_controller()->selected_result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Try navigating the results - this should fail while result update is in
  // progress.
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(selection,
            result_page_view->result_selection_controller()->selected_result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Finish results update - this should reset the selection.
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test"), selection->result()->title());

  // Moving down again should change the selected result.
  KeyPress(ui::VKEY_DOWN);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());
}

// Tests that traversing search results is disabled while the result that would
// be selected next is being removed from results.
TEST_F(SearchBoxViewTest, ChangeSelectionWhileResultsAreBeingRemoved) {
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  view()->search_box()->SetText(base::ASCIIToUTF16("test"));
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7,
                     base::ASCIIToUTF16("tester"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("testing"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      view()->contents_view()->search_results_page_view();

  const SearchResultBaseView* selection =
      result_page_view->result_selection_controller()->selected_result();

  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Remove current results and add a new one - the selection controller is
  // updated asynchronously, so the result is expected to remain the same until
  // the loop is run.
  results()->RemoveAll();
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.,
                     base::ASCIIToUTF16("test"), base::string16());
  EXPECT_EQ(selection,
            result_page_view->result_selection_controller()->selected_result());
  EXPECT_FALSE(selection->result());

  // Try navigating the results - this should fail while result update is in
  // progress.
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(selection,
            result_page_view->result_selection_controller()->selected_result());

  // Finish results update - this should reset the selection.
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test"), selection->result()->title());

  // Moving down should clear the selection (as focus is moved to close button).
  KeyPress(ui::VKEY_DOWN);
  EXPECT_FALSE(
      result_page_view->result_selection_controller()->selected_result());
}

TEST_F(SearchBoxViewTest, UserSelectionNotOverridenByNewResults) {
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  view()->search_box()->SetText(base::ASCIIToUTF16("test"));
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7,
                     base::ASCIIToUTF16("tester"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("testing"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      view()->contents_view()->search_results_page_view();

  const SearchResultBaseView* selection =
      result_page_view->result_selection_controller()->selected_result();

  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Navigate down to select non-default result.
  KeyPress(ui::VKEY_DOWN);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Add a new result - verify the selected result remains the same.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 0.9,
                       base::ASCIIToUTF16("test1"), base::string16());
  // Finish results update.
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     base::ASCIIToUTF16("testing almost"), base::string16());
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Go up.
  KeyPress(ui::VKEY_UP);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Remove the last result, and verify the selection remains the same.
  results()->RemoveAt(3);
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Result should be reset if the selected result is removed.
  results()->RemoveAt(1);
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("test1"), selection->result()->title());

  // New result can override the default selection.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1.0,
                       base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("test"), selection->result()->title());
}

TEST_F(SearchBoxViewTest,
       UserSelectionInNonDefaultContainerNotOverridenByNewResults) {
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  view()->search_box()->SetText(base::ASCIIToUTF16("test"));
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7,
                     base::ASCIIToUTF16("tester"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("testing"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      view()->contents_view()->search_results_page_view();

  const SearchResultBaseView* selection =
      result_page_view->result_selection_controller()->selected_result();

  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("tester"), selection->result()->title());

  // Navigate down to select non-default result.
  KeyPress(ui::VKEY_DOWN);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Add a new result in a tile container - verify the selected result remains
  // the same.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kTile, 0.9,
                       base::ASCIIToUTF16("test tile"), base::string16());
  // Finish results update.
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     base::ASCIIToUTF16("testing almost"), base::string16());
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Remove the result before the selected one, and verify the selection remains
  // the same.
  results()->RemoveAt(1);
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("testing"), selection->result()->title());

  // Result should be reset if the selected result is removed.
  results()->RemoveAt(1);
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("test tile"), selection->result()->title());

  // New result can override the default selection.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kTile, 1.0,
                       base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(base::ASCIIToUTF16("test"), selection->result()->title());
}

// Tests that the default selection is reset after resetting and reactivating
// the search box.
TEST_F(SearchBoxViewTest, ResetSelectionAfterResettingSearchBox) {
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7,
                     base::ASCIIToUTF16("test1"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("test2"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      view()->contents_view()->search_results_page_view();

  // Selection should rest on the first result, which is default.
  const SearchResultBaseView* selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test1"), selection->result()->title());
  EXPECT_TRUE(selection->is_default_result());

  // Navigate down then up. The first result should no longer be default.
  KeyPress(ui::VKEY_DOWN);
  KeyPress(ui::VKEY_UP);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test1"), selection->result()->title());
  EXPECT_FALSE(selection->is_default_result());

  // Navigate down to the second result.
  KeyPress(ui::VKEY_DOWN);

  selection =
      result_page_view->result_selection_controller()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test2"), selection->result()->title());

  // Reset the search box.
  view()->ClearSearchAndDeactivateSearchBox();
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  result_page_view->OnSearchResultContainerResultsChanged();

  // Selection should again rest on the first result, which is default.
  selection =
      result_page_view->result_selection_controller()->selected_result();
  EXPECT_EQ(result_page_view->first_result_view(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(base::ASCIIToUTF16("test1"), selection->result()->title());
  EXPECT_TRUE(selection->is_default_result());
}

TEST_F(SearchBoxViewTest, NewSearchQueryActionRecordedWhenUserType) {
  base::UserActionTester user_action_tester;
  // User starts to type a character in search box.
  KeyPress(ui::VKEY_A);
  EXPECT_EQ(1, user_action_tester.GetActionCount("AppList_SearchQueryStarted"));

  // User continues to type another character.
  KeyPress(ui::VKEY_B);
  EXPECT_EQ(1, user_action_tester.GetActionCount("AppList_SearchQueryStarted"));

  // User erases the query in the search box and types a new one.
  KeyPress(ui::VKEY_BACK);
  KeyPress(ui::VKEY_BACK);
  KeyPress(ui::VKEY_C);
  EXPECT_EQ(2, user_action_tester.GetActionCount("AppList_SearchQueryStarted"));
}

TEST_F(SearchBoxViewTest, NavigatePrivacyNotice) {
  // Create a new contents view that contains a privacy notice.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  auto* contents_view = widget()->GetContentsView()->AddChildView(
      std::make_unique<KeyPressCounterView>(app_list_view()));
  contents_view->Init(view_delegate()->GetModel());
  view()->set_contents_view(contents_view);

  PrivacyContainerView* const privacy_container_view =
      contents_view->privacy_container_view();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  SearchResultPageView* const result_page_view =
      contents_view->search_results_page_view();
  ResultSelectionController* const selection_controller =
      result_page_view->result_selection_controller();

  // The privacy view should be selected by default.
  const SearchResultBaseView* selection =
      selection_controller->selected_result();
  EXPECT_TRUE(selection);
  EXPECT_TRUE(selection->is_default_result());
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  // The privacy view should have one additional action.
  KeyPress(ui::VKEY_TAB);
  selection = selection_controller->selected_result();
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  KeyPress(ui::VKEY_TAB);
  selection = selection_controller->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(selection->result()->title(), base::ASCIIToUTF16("test"));

  // The privacy notice should also have two actions when navigating backwards.
  KeyPress(ui::VKEY_TAB, /*is_shift_down=*/true);
  selection = selection_controller->selected_result();
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  KeyPress(ui::VKEY_TAB, /*is_shift_down=*/true);
  selection = selection_controller->selected_result();
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  KeyPress(ui::VKEY_TAB, /*is_shift_down=*/true);
  selection = selection_controller->selected_result();
  EXPECT_FALSE(selection);
}

TEST_F(SearchBoxViewTest, KeyboardEventClosesPrivacyNotice) {
  // Create a new contents view that contains a privacy notice.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  auto* contents_view = widget()->GetContentsView()->AddChildView(
      std::make_unique<KeyPressCounterView>(app_list_view()));
  contents_view->Init(view_delegate()->GetModel());
  view()->set_contents_view(contents_view);

  PrivacyContainerView* const privacy_container_view =
      contents_view->privacy_container_view();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(contents_view->search_results_page_view()
                ->result_selection_controller()
                ->selected_result(),
            privacy_container_view->GetResultViewAt(0));

  // Navigate to the close button and press enter. The privacy info should no
  // longer be shown.
  KeyPress(ui::VKEY_TAB);
  KeyPress(ui::VKEY_RETURN);
  EXPECT_FALSE(view_delegate()->ShouldShowAssistantPrivacyInfo());
}

TEST_F(SearchBoxViewTest, PrivacyViewActionNotOverriddenByNewResults) {
  // Create a new contents view that contains a privacy notice.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  auto* contents_view = widget()->GetContentsView()->AddChildView(
      std::make_unique<KeyPressCounterView>(app_list_view()));
  contents_view->Init(view_delegate()->GetModel());
  view()->set_contents_view(contents_view);

  PrivacyContainerView* const privacy_container_view =
      contents_view->privacy_container_view();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  ResultSelectionController* const selection_controller =
      contents_view->search_results_page_view()->result_selection_controller();
  const SearchResultBaseView* selection =
      selection_controller->selected_result();
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  // The privacy view should have one additional action. Tab to the next privacy
  // view action.
  KeyPress(ui::VKEY_TAB);
  selection = selection_controller->selected_result();
  EXPECT_EQ(selection, privacy_container_view->GetResultViewAt(0));

  // Create a new search result. The privacy view should have no actions
  // remaining.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("testing"), base::string16());
  base::RunLoop().RunUntilIdle();

  KeyPress(ui::VKEY_TAB);
  selection = selection_controller->selected_result();
  ASSERT_TRUE(selection);
  EXPECT_EQ(selection->result()->title(), base::ASCIIToUTF16("test"));
}

TEST_F(SearchBoxViewTest, PrivacySelectionDoesNotChangeSearchBoxText) {
  // Create a new contents view that contains a privacy notice.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  auto* contents_view = widget()->GetContentsView()->AddChildView(
      std::make_unique<KeyPressCounterView>(app_list_view()));
  contents_view->Init(view_delegate()->GetModel());
  view()->set_contents_view(contents_view);

  PrivacyContainerView* const privacy_container_view =
      contents_view->privacy_container_view();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("test"), base::string16());
  base::RunLoop().RunUntilIdle();

  ResultSelectionController* const selection_controller =
      contents_view->search_results_page_view()->result_selection_controller();
  EXPECT_EQ(selection_controller->selected_result(),
            privacy_container_view->GetResultViewAt(0));
  EXPECT_TRUE(view()->search_box()->GetText().empty());

  // Navigate to a search result and back to the privacy notice. The text should
  // not be reset.
  KeyPress(ui::VKEY_DOWN);
  const SearchResultBaseView* selection =
      selection_controller->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(selection->result()->title(), base::ASCIIToUTF16("test"));
  EXPECT_EQ(base::ASCIIToUTF16("test"), view()->search_box()->GetText());

  KeyPress(ui::VKEY_UP);
  EXPECT_EQ(selection_controller->selected_result(),
            privacy_container_view->GetResultViewAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("test"), view()->search_box()->GetText());
}

class SearchBoxViewAssistantButtonTest : public SearchBoxViewTest {
 public:
  SearchBoxViewAssistantButtonTest() = default;
  ~SearchBoxViewAssistantButtonTest() override = default;

  // Overridden from testing::Test
  void SetUp() override {
    SearchBoxViewTest::SetUp();
    view_delegate()->GetSearchModel()->search_box()->SetShowAssistantButton(
        true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxViewAssistantButtonTest);
};

// Tests that the assistant button is visible by default.
TEST_F(SearchBoxViewAssistantButtonTest, AssistantButtonVisibleByDefault) {
  EXPECT_TRUE(view()->assistant_button()->GetVisible());
}

// Tests that the assistant button is visible after the search box is activated.
TEST_F(SearchBoxViewAssistantButtonTest,
       AssistantButtonVisibleAfterSearchBoxActived) {
  // Assistant button is not showing up under zero state for now.
  // TODO(jennyz): Make assistant button show up under zero state.
  if (!app_list_features::IsZeroStateSuggestionsEnabled()) {
    SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
    EXPECT_TRUE(view()->assistant_button()->GetVisible());
  }
}

// Tests that the assistant button is invisible after typing in the search box,
// and comes back when search box is empty.
TEST_F(SearchBoxViewAssistantButtonTest,
       AssistantButtonChangeVisibilityWithTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_FALSE(view()->assistant_button()->GetVisible());

  // Assistant button is not showing up under zero state for now.
  // TODO(crbug.com/925455): Make assistant button show up under zero state.
  if (!app_list_features::IsZeroStateSuggestionsEnabled()) {
    KeyPress(ui::VKEY_BACK);
    EXPECT_TRUE(view()->assistant_button()->GetVisible());
  }
}

class SearchBoxViewAutocompleteTest
    : public SearchBoxViewTest,
      public ::testing::WithParamInterface<ui::KeyboardCode> {
 public:
  SearchBoxViewAutocompleteTest() = default;
  ~SearchBoxViewAutocompleteTest() override = default;

  // Overridden from testing::Test
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {app_list_features::kEnableAppListSearchAutocomplete}, {});
    SearchBoxViewTest::SetUp();
  }

  // Expect the entire autocomplete suggestion if |should_autocomplete| is true,
  // expect only typed characters otherwise.
  void ExpectAutocompleteSuggestion(bool should_autocomplete) {
    if (should_autocomplete) {
      // Search box autocomplete suggestion is accepted, but it should not
      // trigger another query, thus it is not reflected in Search Model.
      EXPECT_EQ(base::ASCIIToUTF16("hello world!"),
                view()->search_box()->GetText());
      EXPECT_EQ(base::ASCIIToUTF16("he"),
                view_delegate()->GetSearchModel()->search_box()->text());
    } else {
      // Search box autocomplete suggestion is removed and is reflected in
      // SearchModel.
      EXPECT_EQ(view()->search_box()->GetText(),
                view_delegate()->GetSearchModel()->search_box()->text());
      EXPECT_EQ(base::ASCIIToUTF16("he"), view()->search_box()->GetText());
      // ProcessAutocomplete should be a no-op.
      view()->ProcessAutocomplete();
      // The autocomplete suggestion should still not be present.
      EXPECT_EQ(base::ASCIIToUTF16("he"), view()->search_box()->GetText());
    }
  }

  // Sets up the test by creating a SearchResult and displaying an autocomplete
  // suggestion.
  void SetupAutocompleteBehaviorTest() {
    // Add a search result with a non-empty title field.
    CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                       base::ASCIIToUTF16("hello world!"), base::string16());
    base::RunLoop().RunUntilIdle();

    // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
    KeyPress(ui::VKEY_H);
    KeyPress(ui::VKEY_E);
    view()->ProcessAutocomplete();
  }

  // Clears all existing text from search_box() and all existing SearchResults
  // from results().
  void ResetAutocompleteBehaviorTest() {
    view()->search_box()->SetText(base::string16());
    results()->RemoveAll();
  }

  // Test a GestureEvent's autocomplete behavior according to
  // |should_autocomplete|. Expect the entire autocomplete suggestion if
  // |should_autocomplete| is true, expect only typed characters otherwise.
  void TestGestureEvent(const ui::GestureEvent& gesture_event,
                        bool should_autocomplete) {
    SetupAutocompleteBehaviorTest();
    // Forward |gesture_event| to HandleGestureEvent() directly because we
    // cannot test GestureEvents properly due to not having ash dependencies.
    // Static cast to TextfieldController because HandleGestureEvent() is
    // private in SearchBoxView. TODO(crbug.com/878984): Derive
    // SearchBoxViewTest from AshTestBase in order to test events using
    // EventGenerator instead.
    static_cast<views::TextfieldController*>(view())->HandleGestureEvent(
        view()->search_box(), gesture_event);
    ExpectAutocompleteSuggestion(should_autocomplete);
    // Reset search box text and SearchResults for next test.
    ResetAutocompleteBehaviorTest();
  }

  // Test a KeyEvent's autocomplete behavior according to |should_autocomplete|.
  // Expect the entire autocomplete suggestion if |should_autocomplete| is true,
  // expect only typed characters otherwise.
  void TestKeyEvent(const ui::KeyEvent& key_event, bool should_autocomplete) {
    SetupAutocompleteBehaviorTest();
    // TODO(crbug.com/878984): Change KeyPress() to use EventGenerator::PressKey
    // instead.
    if (key_event.key_code() == ui::VKEY_BACK) {
      // Use KeyPress() to mimic backspace. HandleKeyEvent() will not delete the
      // text.
      KeyPress(key_event.key_code());
    } else {
      // Forward |key_event| to HandleKeyEvent(). We use HandleKeyEvent()
      // because KeyPress() will replace the existing highlighted text. Static
      // cast to TextfieldController because HandleGestureEvent() is private in
      // SearchBoxView.
      static_cast<views::TextfieldController*>(view())->HandleKeyEvent(
          view()->search_box(), key_event);
    }
    ExpectAutocompleteSuggestion(should_autocomplete);
    // Reset search box text and SearchResults for next test.
    ResetAutocompleteBehaviorTest();
  }

  // Test a MouseEvent's autocomplete behavior according to
  // |should_autocomplete|. Expect the entire autocomplete suggestion if
  // |should_autocomplete| is true, expect only typed characters otherwise.
  void TestMouseEvent(const ui::MouseEvent& mouse_event,
                      bool should_autocomplete) {
    SetupAutocompleteBehaviorTest();
    // Forward |mouse_event| to HandleMouseEvent() directly because we cannot
    // test MouseEvents properly due to not having ash dependencies. Static cast
    // to TextfieldController because HandleGestureEvent() is a private method
    // in SearchBoxView. TODO(crbug.com/878984): Derive SearchBoxViewTest from
    // AshTestBase in order to test events using EventGenerator instead.
    static_cast<views::TextfieldController*>(view())->HandleMouseEvent(
        view()->search_box(), mouse_event);
    ExpectAutocompleteSuggestion(should_autocomplete);
    // Reset search box text and SearchResults for next test.
    ResetAutocompleteBehaviorTest();
  }

  ui::KeyboardCode key_code() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxViewAutocompleteTest);
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchBoxViewAutocompleteTest,
                         ::testing::Values(ui::VKEY_LEFT,
                                           ui::VKEY_RIGHT,
                                           ui::VKEY_UP,
                                           ui::VKEY_DOWN,
                                           ui::VKEY_BACK));

// Tests that autocomplete suggestions are consistent with top SearchResult list
// titles.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultTitle) {
  // Add two SearchResults, one tile and one list result. Initialize their title
  // field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("hello list"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 0.5,
                     base::ASCIIToUTF16("hello tile"), base::string16());
  base::RunLoop().RunUntilIdle();

  // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->ProcessAutocomplete();

  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("hello tile"));
  EXPECT_EQ(view()->search_box()->GetSelectedText(),
            base::ASCIIToUTF16("llo tile"));
}

// Tests that autocomplete suggestions are consistent with top SearchResult tile
// titles.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopTileResultTitle) {
  // Add two SearchResults, one tile and one list result. Initialize their title
  // field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 1.0,
                     base::ASCIIToUTF16("hello tile"), base::string16());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     base::ASCIIToUTF16("hello list"), base::string16());
  base::RunLoop().RunUntilIdle();

  // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("hello tile"));
  EXPECT_EQ(view()->search_box()->GetSelectedText(),
            base::ASCIIToUTF16("llo tile"));
}

// Tests that autocomplete suggestions are consistent with top SearchResult list
// details.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultDetails) {
  // Add two SearchResults, one tile and one list result. The tile should
  // display first, despite having a lower score. Initialize their details field
  // to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, base::string16(),
                     base::ASCIIToUTF16("hello list"));
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 0.5, base::string16(),
                     base::ASCIIToUTF16("hello tile"));
  base::RunLoop().RunUntilIdle();

  // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("hello tile"));
  EXPECT_EQ(view()->search_box()->GetSelectedText(),
            base::ASCIIToUTF16("llo tile"));
}

// Tests that autocomplete suggestions are consistent with top SearchResult tile
// details.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopTileResultDetails) {
  // Add two SearchResults, one tile and one list result. Initialize their
  // details field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 1.0, base::string16(),
                     base::ASCIIToUTF16("hello tile"));
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, base::string16(),
                     base::ASCIIToUTF16("hello list"));
  base::RunLoop().RunUntilIdle();

  // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("hello tile"));
  EXPECT_EQ(view()->search_box()->GetSelectedText(),
            base::ASCIIToUTF16("llo tile"));
}

// Tests that SearchBoxView's textfield text does not autocomplete if the top
// result title or details do not have a matching prefix.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxDoesNotAutocompleteWrongCharacter) {
  // Add a search result with non-empty details and title fields.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("title"),
                     base::ASCIIToUTF16("details"));
  base::RunLoop().RunUntilIdle();

  // Send Z to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_Z);
  view()->ProcessAutocomplete();
  // The text should not be autocompleted.
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("z"));
}

// Tests that autocomplete suggestion will remain if next key in the suggestion
// is typed.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesAcceptsNextChar) {
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("hello world!"), base::string16());
  base::RunLoop().RunUntilIdle();

  // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->ProcessAutocomplete();

  // After typing L, the highlighted text will be replaced by L.
  KeyPress(ui::VKEY_L);
  base::string16 selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("hel"));
  EXPECT_EQ(base::ASCIIToUTF16(""), selected_text);

  // After handling autocomplete, the highlighted text will show again.
  view()->ProcessAutocomplete();
  selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(),
            base::ASCIIToUTF16("hello world!"));
  EXPECT_EQ(base::ASCIIToUTF16("lo world!"), selected_text);
}

// Tests that autocomplete suggestion is accepted and displayed in SearchModel
// after clicking or tapping on the search box.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForClickTap) {
  TestMouseEvent(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                gfx::Point(), ui::EventTimeForNow(), 0, 0),
                 true);
  TestGestureEvent(
      ui::GestureEvent(0, 0, 0, ui::EventTimeForNow(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP)),
      true);
}

// Tests that autocomplete is not handled if IME is using composition text.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesNotHandledForIME) {
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                     base::ASCIIToUTF16("hello world!"), base::string16());
  base::RunLoop().RunUntilIdle();

  // Simulate uncomposited text. The autocomplete should be handled.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->set_highlight_range_for_test(gfx::Range(2, 2));
  view()->ProcessAutocomplete();

  base::string16 selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(),
            base::ASCIIToUTF16("hello world!"));
  EXPECT_EQ(base::ASCIIToUTF16("llo world!"), selected_text);
  view()->search_box()->SetText(base::string16());

  // Simulate IME composition text. The autocomplete should not be handled.
  ui::CompositionText composition_text;
  composition_text.text = base::ASCIIToUTF16("he");
  view()->search_box()->SetCompositionText(composition_text);
  view()->set_highlight_range_for_test(gfx::Range(2, 2));
  view()->ProcessAutocomplete();

  selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), base::ASCIIToUTF16("he"));
  EXPECT_EQ(base::ASCIIToUTF16(""), selected_text);
}

}  // namespace test
}  // namespace ash
