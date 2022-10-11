// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view_delegate.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_mixer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"

namespace {
// kBestMatch is the second result container for productivity launcher search.
constexpr int kBestMatchIndex = 1;

bool IsValidSearchBoxAccessibilityHint(const std::u16string& hint) {
  SCOPED_TRACE(testing::Message() << "Hint Text: " << hint);
  // Search box placeholder text is randomly selected for productivity
  // launcher.
  std::vector<std::u16string> possible_a11y_text = {
      l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SHORTCUTS)),
      l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SETTINGS)),
      l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
          l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS))};
  // Check if the current accessibility text is one of the possible
  // options.
  return base::Contains(possible_a11y_text, hint);
}

}  // namespace

namespace ash {
namespace {

using test::AppListTestViewDelegate;

SearchModel* GetSearchModel() {
  return AppListModelProvider::Get()->search_model();
}

class KeyPressCounterView : public ContentsView {
 public:
  explicit KeyPressCounterView(AppListView* app_list_view)
      : ContentsView(app_list_view), count_(0) {}

  KeyPressCounterView(const KeyPressCounterView&) = delete;
  KeyPressCounterView& operator=(const KeyPressCounterView&) = delete;

  ~KeyPressCounterView() override = default;

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
};

class SearchBoxViewTest : public views::test::WidgetTest,
                          public SearchBoxViewDelegate,
                          public testing::WithParamInterface<bool> {
 public:
  SearchBoxViewTest() = default;

  SearchBoxViewTest(const SearchBoxViewTest&) = delete;
  SearchBoxViewTest& operator=(const SearchBoxViewTest&) = delete;

  ~SearchBoxViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kProductivityLauncher,
                                              IsProductivityLauncherEnabled());

    views::test::WidgetTest::SetUp();

    // Tests have an implicit dependency on the color providers.
    ui::ColorProviderManager::Get().AppendColorProviderInitializer(
        base::BindRepeating(AddCrosStylesColorMixer));
    ui::ColorProviderManager::Get().AppendColorProviderInitializer(
        base::BindRepeating(AddAshColorMixer));

    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));

    std::unique_ptr<SearchBoxView> view;
    if (IsProductivityLauncherEnabled()) {
      // Initialize SearchBoxView like clamshell productivity launcher.
      view = std::make_unique<SearchBoxView>(this, &view_delegate_,
                                             /*app_list_view=*/nullptr);
      view->InitializeForBubbleLauncher();
      view_ = widget_->GetContentsView()->AddChildView(std::move(view));

      productivity_launcher_search_view_ =
          widget_->GetContentsView()->AddChildView(
              std::make_unique<ProductivityLauncherSearchView>(
                  &view_delegate_, /*dialog_controller=*/nullptr, view_));
      widget_->Show();
    } else {
      // Initialize SearchBoxView like peeking launcher.
      app_list_view_ = new AppListView(&view_delegate_);
      app_list_view_->InitView(GetContext());
      view_ = app_list_view_->search_box_view();
      app_list_view_->Show(AppListViewState::kFullscreenAllApps, false);
    }
  }

  void TearDown() override {
    ui::ColorProviderManager::ResetForTesting();
    if (app_list_view_)
      app_list_view_->GetWidget()->Close();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

  bool IsProductivityLauncherEnabled() const { return GetParam(); }

 protected:
  views::Widget* widget() { return widget_; }
  SearchBoxView* view() { return view_; }
  AppListView* app_list_view() { return app_list_view_; }
  AppListTestViewDelegate* view_delegate() { return &view_delegate_; }

  void SetSearchEngineIsGoogle(bool is_google) {
    view_delegate_.SetSearchEngineIsGoogle(is_google);
  }

  void SetSearchBoxActive(bool active, ui::EventType type) {
    view()->SetSearchBoxActive(active, type);
  }

  void SetContentsView(ContentsView* contents_view) {
    view()->set_contents_view(contents_view);
    view()->SetResultSelectionController(
        contents_view->search_result_page_view()
            ->result_selection_controller());
  }

  void KeyPress(ui::KeyboardCode key_code, bool is_shift_down = false) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code,
                       is_shift_down ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    view()->search_box()->OnKeyEvent(&event);
    // Emulates the input method.
    if (::isalnum(static_cast<int>(key_code))) {
      char16_t character = ::tolower(static_cast<int>(key_code));
      view()->search_box()->InsertText(
          std::u16string(1, character),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    }
  }

  // Creates a SearchResult with the given parameters.
  void CreateSearchResult(ash::SearchResultDisplayType display_type,
                          double display_score,
                          const std::u16string& title,
                          const std::u16string& details) {
    CreateSearchResultAt(results()->item_count(), display_type, display_score,
                         title, details);
  }

  // Creates a SearchResult with the given parameters at the given index in
  // the results list.
  void CreateSearchResultAt(size_t index,
                            ash::SearchResultDisplayType display_type,
                            double display_score,
                            const std::u16string& title,
                            const std::u16string& details) {
    auto search_result = std::make_unique<TestSearchResult>();
    search_result->set_result_id(base::NumberToString(++last_result_id_));
    search_result->set_display_type(display_type);
    search_result->set_display_score(display_score);
    search_result->SetTitle(title);
    search_result->SetDetails(details);
    search_result->set_best_match(true);
    results()->AddAt(index, std::move(search_result));
  }

  SearchModel::SearchResults* results() { return GetSearchModel()->results(); }

  SearchResultPageView* GetSearchResultPageView() {
    DCHECK(!IsProductivityLauncherEnabled());
    return app_list_view()
        ->app_list_main_view()
        ->contents_view()
        ->search_result_page_view();
  }
  SearchResultBaseView* GetFirstResultView() {
    if (IsProductivityLauncherEnabled()) {
      return productivity_launcher_search_view_
          ->result_container_views_for_test()[kBestMatchIndex]
          ->GetFirstResultView();
    }
    return GetSearchResultPageView()->first_result_view();
  }

  ResultSelectionController* GetResultSelectionController() {
    if (IsProductivityLauncherEnabled()) {
      return productivity_launcher_search_view_
          ->result_selection_controller_for_test();
    }
    return GetSearchResultPageView()->result_selection_controller();
  }

  void OnSearchResultContainerResultsChanged() {
    if (IsProductivityLauncherEnabled()) {
      productivity_launcher_search_view_
          ->OnSearchResultContainerResultsChanged();
    } else {
      return GetSearchResultPageView()->OnSearchResultContainerResultsChanged();
    }
  }

  void SimulateQuery(const std::u16string& query) {
    view()->search_box()->InsertText(
        query,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(const std::u16string& trimmed_query,
                    bool initiated_by_user) override {
    if (IsProductivityLauncherEnabled()) {
      productivity_launcher_search_view_->UpdateForNewSearch(
          !trimmed_query.empty());
    }
  }
  void AssistantButtonPressed() override {}
  void CloseButtonPressed() override {}
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override {}
  bool CanSelectSearchResults() override { return true; }

  AshColorProvider ash_color_provider_;
  ProductivityLauncherSearchView* productivity_launcher_search_view_ = nullptr;
  TestAppListColorProvider color_provider_;  // Needed by AppListView.
  AppListTestViewDelegate view_delegate_;
  views::Widget* widget_ = nullptr;
  AppListView* app_list_view_ = nullptr;
  SearchBoxView* view_ = nullptr;                // Owned by views hierarchy.
  KeyPressCounterView* counter_view_ = nullptr;  // Owned by views hierarchy.
  int last_result_id_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Run search box view tests with and without productivity launcher enabled.
INSTANTIATE_TEST_SUITE_P(All, SearchBoxViewTest, testing::Bool());

class SearchBoxViewWithSuggestedContentTest : public SearchBoxViewTest {
 public:
  void SetUp() override {
    view_delegate_.SetShouldShowSuggestedContentInfo(true);
    SearchBoxViewTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchBoxViewWithSuggestedContentTest,
                         testing::Values(false));

TEST_P(SearchBoxViewTest, SearchBoxTextUsesAppListSearchBoxTextColor) {
  // With darklight mode enabled by default, search box text color should be the
  // same with and without productivity launcher enabled.
  EXPECT_EQ(view()->search_box()->GetTextColor(), gfx::kGoogleGrey900);
}

// Tests that the close button is invisible by default.
TEST_P(SearchBoxViewTest, CloseButtonInvisibleByDefault) {
  EXPECT_FALSE(view()->close_button()->GetVisible());
}

// Tests that the close button becomes visible after typing in the search box.
TEST_P(SearchBoxViewTest, CloseButtonVisibleAfterTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_TRUE(view()->close_button()->GetVisible());
}

// Tests that the close button is still visible after the search box is
// activated (in zero state).
TEST_P(SearchBoxViewTest, CloseButtonVisibleInZeroStateSearchBox) {
  SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  EXPECT_EQ(!IsProductivityLauncherEnabled(),
            view()->close_button()->GetVisible());
}

// Tests that the search box is inactive by default.
TEST_P(SearchBoxViewTest, SearchBoxInactiveByDefault) {
  // ProductivityLauncher has the search box active by default. This is tested
  // elsewhere.
  if (IsProductivityLauncherEnabled())
    return;

  ASSERT_FALSE(view()->is_search_box_active());
}

TEST_P(SearchBoxViewTest, AccessibilityHintRemovedWhenSearchBoxActive) {
  if (IsProductivityLauncherEnabled()) {
    EXPECT_TRUE(IsValidSearchBoxAccessibilityHint(
        view()->search_box()->GetAccessibleName()));
    SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
    EXPECT_TRUE(IsValidSearchBoxAccessibilityHint(
        view()->search_box()->GetAccessibleName()));
  } else {
    EXPECT_EQ(view()->search_box()->GetAccessibleName(),
              l10n_util::GetStringUTF16(
                  IDS_APP_LIST_SEARCH_BOX_ACCESSIBILITY_NAME_CLAMSHELL));
    SetSearchBoxActive(
        true, ui::ET_MOUSE_PRESSED);  // In the non-bubble launcher, when the
                                      // search box is active there are no
    // apps to navigate with arrow keys, so remove the accessibility hint.
    EXPECT_EQ(view()->search_box()->GetAccessibleName(), u"");
  }
}

// Tests that the black Google icon is used for an inactive Google search.
TEST_P(SearchBoxViewTest, SearchBoxInactiveSearchBoxGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kGoogleBlackIcon, view()->GetSearchBoxIconSize(), kDefaultSearchboxColor);

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the colored Google icon is used for an active Google search.
TEST_P(SearchBoxViewTest, SearchBoxActiveSearchEngineGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      vector_icons::kGoogleColorIcon, view()->GetSearchBoxIconSize(),
      kDefaultSearchboxColor);

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an inactive non-Google search.
TEST_P(SearchBoxViewTest, SearchBoxInactiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, view()->GetSearchBoxIconSize(),
      kDefaultSearchboxColor);

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an active non-Google search.
TEST_P(SearchBoxViewTest, SearchBoxActiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, view()->GetSearchBoxIconSize(),
      kDefaultSearchboxColor);

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that traversing search results is disabled while results are being
// updated.
TEST_P(SearchBoxViewTest, ChangeSelectionWhileResultsAreChanging) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  const SearchResultBaseView* selection =
      GetResultSelectionController()->selected_result();

  ASSERT_TRUE(selection);
  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Add a new result - the selection controller is updated asynchronously, so
  // the result is expected to remain the same until the loop is run.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1., u"test",
                       std::u16string());
  EXPECT_EQ(selection, GetResultSelectionController()->selected_result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Try navigating the results - this should fail while result update is in
  // progress.
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(selection, GetResultSelectionController()->selected_result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Finish results update - this should reset the selection.
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test", selection->result()->title());

  // Moving down again should change the selected result.
  KeyPress(ui::VKEY_DOWN);

  selection = GetResultSelectionController()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"tester", selection->result()->title());
}

// Tests that traversing search results is disabled while the result that would
// be selected next is being removed from results.
TEST_P(SearchBoxViewTest, ChangeSelectionWhileResultsAreBeingRemoved) {
  SimulateQuery(u"test");

  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  const SearchResultBaseView* selection =
      GetResultSelectionController()->selected_result();

  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Remove current results and add a new one - the selection controller is
  // updated asynchronously, so the result is expected to remain the same until
  // the loop is run.
  results()->RemoveAll();
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1., u"test",
                     std::u16string());
  EXPECT_EQ(selection, GetResultSelectionController()->selected_result());
  EXPECT_FALSE(selection->result());

  // Try navigating the results - this should fail while result update is in
  // progress.
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(selection, GetResultSelectionController()->selected_result());

  // Finish results update - this should reset the selection.
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test", selection->result()->title());

  // Moving down should clear the selection (as focus is moved to close button).
  KeyPress(ui::VKEY_DOWN);
  EXPECT_FALSE(GetResultSelectionController()->selected_result());
}

TEST_P(SearchBoxViewTest, UserSelectionNotOverridenByNewResults) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  const SearchResultBaseView* selection =
      GetResultSelectionController()->selected_result();

  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Navigate down to select non-default result.
  KeyPress(ui::VKEY_DOWN);

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Add a new result - verify the selected result remains the same.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 0.9, u"test1",
                       std::u16string());
  // Finish results update.
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     u"testing almost", std::u16string());
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Go up.
  KeyPress(ui::VKEY_UP);

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"tester", selection->result()->title());

  // Remove the last result, and verify the selection remains the same.
  results()->RemoveAt(3);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"tester", selection->result()->title());

  // Result should be reset if the selected result is removed.
  results()->RemoveAt(1);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"test1", selection->result()->title());

  // New result can override the default selection.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1.0, u"test",
                       std::u16string());
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"test", selection->result()->title());
}

TEST_P(SearchBoxViewTest,
       UserSelectionInNonDefaultContainerNotOverridenByNewResults) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  const SearchResultBaseView* selection =
      GetResultSelectionController()->selected_result();

  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"tester", selection->result()->title());

  // Navigate down to select non-default result.
  KeyPress(ui::VKEY_DOWN);

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Tile containers are deprecated for productivity launcher.
  if (!IsProductivityLauncherEnabled()) {
    // Add a new result in a tile container - verify the selected result remains
    // the same.
    CreateSearchResultAt(0, ash::SearchResultDisplayType::kTile, 0.9,
                         u"test tile", std::u16string());
    // Finish results update.
    base::RunLoop().RunUntilIdle();

    selection = GetResultSelectionController()->selected_result();
    EXPECT_EQ(u"testing", selection->result()->title());
  }

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     u"testing almost", std::u16string());
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Remove the result before the selected one, and verify the selection remains
  // the same.
  results()->RemoveAt(IsProductivityLauncherEnabled() ? 0 : 1);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Result should be reset if the selected result is removed.
  results()->RemoveAt(IsProductivityLauncherEnabled() ? 0 : 1);
  base::RunLoop().RunUntilIdle();

  // Tile results are not created when testing productivity launcher.
  selection = GetResultSelectionController()->selected_result();
  if (IsProductivityLauncherEnabled()) {
    EXPECT_EQ(u"testing almost", selection->result()->title());

    // New result can override the default selection.
    CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1.0, u"test",
                         std::u16string());
  } else {
    EXPECT_EQ(u"test tile", selection->result()->title());

    // New result can override the default selection.
    CreateSearchResultAt(0, ash::SearchResultDisplayType::kTile, 1.0, u"test",
                         std::u16string());
  }
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"test", selection->result()->title());
}

// Tests that the default selection is reset after resetting and reactivating
// the search box.
TEST_P(SearchBoxViewTest, ResetSelectionAfterResettingSearchBox) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"test1",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"test2",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  auto* result_selection_controller = GetResultSelectionController();

  // Selection should rest on the first result, which is default.
  const SearchResultBaseView* selection =
      result_selection_controller->selected_result();
  ASSERT_TRUE(selection);
  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test1", selection->result()->title());
  EXPECT_TRUE(selection->is_default_result());

  // Navigate down then up. The first result should no longer be default.
  KeyPress(ui::VKEY_DOWN);
  KeyPress(ui::VKEY_UP);

  selection = result_selection_controller->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test1", selection->result()->title());
  EXPECT_FALSE(selection->is_default_result());

  // Navigate down to the second result.
  KeyPress(ui::VKEY_DOWN);

  selection = result_selection_controller->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test2", selection->result()->title());

  // Reset the search box.
  view()->ClearSearchAndDeactivateSearchBox();
  SetSearchBoxActive(true, ui::ET_UNKNOWN);

  // Productivity launcher search results are not shown when the search box
  // is empty. Tested elsewhere.
  if (IsProductivityLauncherEnabled())
    return;

  OnSearchResultContainerResultsChanged();
  // Selection should again reset on the first result, which is default.
  selection = result_selection_controller->selected_result();
  EXPECT_EQ(GetFirstResultView(), selection);
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(u"test1", selection->result()->title());
  EXPECT_TRUE(selection->is_default_result());
}

TEST_P(SearchBoxViewTest, NewSearchQueryActionRecordedWhenUserType) {
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

TEST_P(SearchBoxViewWithSuggestedContentTest, NavigateSuggestedContentInfo) {
  PrivacyContainerView* const privacy_container_view =
      GetSearchResultPageView()->GetPrivacyContainerViewForTest();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"test",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ResultSelectionController* const selection_controller =
      GetResultSelectionController();

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
  EXPECT_EQ(selection->result()->title(), u"test");

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

TEST_P(SearchBoxViewWithSuggestedContentTest,
       KeyboardEventClosesSuggestedContentInfo) {
  PrivacyContainerView* const privacy_container_view =
      GetSearchResultPageView()->GetPrivacyContainerViewForTest();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"test",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetSearchResultPageView()
                ->result_selection_controller()
                ->selected_result(),
            privacy_container_view->GetResultViewAt(0));

  // Navigate to the close button and press enter. The suggested content info
  // should no longer be shown.
  KeyPress(ui::VKEY_TAB);
  KeyPress(ui::VKEY_RETURN);
  EXPECT_FALSE(view_delegate()->ShouldShowSuggestedContentInfo());
}

TEST_P(SearchBoxViewWithSuggestedContentTest,
       SuggestedContentActionNotOverriddenByNewResults) {
  PrivacyContainerView* const privacy_container_view =
      GetSearchResultPageView()->GetPrivacyContainerViewForTest();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"test",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ResultSelectionController* const selection_controller =
      GetResultSelectionController();
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
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  KeyPress(ui::VKEY_TAB);
  selection = selection_controller->selected_result();
  ASSERT_TRUE(selection);
  EXPECT_EQ(selection->result()->title(), u"test");
}

TEST_P(SearchBoxViewWithSuggestedContentTest,
       SuggestedContentSelectionDoesNotChangeSearchBoxText) {
  PrivacyContainerView* const privacy_container_view =
      GetSearchResultPageView()->GetPrivacyContainerViewForTest();
  ASSERT_TRUE(privacy_container_view);

  // Set up the search box.
  SetSearchBoxActive(true, ui::ET_UNKNOWN);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"test",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ResultSelectionController* const selection_controller =
      GetResultSelectionController();
  EXPECT_EQ(selection_controller->selected_result(),
            privacy_container_view->GetResultViewAt(0));
  EXPECT_TRUE(view()->search_box()->GetText().empty());

  // Navigate to a search result and back to the privacy notice. The text should
  // not be reset.
  KeyPress(ui::VKEY_DOWN);
  const SearchResultBaseView* selection =
      selection_controller->selected_result();
  ASSERT_TRUE(selection->result());
  EXPECT_EQ(selection->result()->title(), u"test");
  EXPECT_EQ(u"test", view()->search_box()->GetText());

  KeyPress(ui::VKEY_UP);
  EXPECT_EQ(selection_controller->selected_result(),
            privacy_container_view->GetResultViewAt(0));
  EXPECT_EQ(u"test", view()->search_box()->GetText());
}

class SearchBoxViewAssistantButtonTest : public SearchBoxViewTest {
 public:
  SearchBoxViewAssistantButtonTest() = default;
  SearchBoxViewAssistantButtonTest(const SearchBoxViewAssistantButtonTest&) =
      delete;
  SearchBoxViewAssistantButtonTest& operator=(
      const SearchBoxViewAssistantButtonTest&) = delete;
  ~SearchBoxViewAssistantButtonTest() override = default;

  // Overridden from testing::Test
  void SetUp() override {
    SearchBoxViewTest::SetUp();
    GetSearchModel()->search_box()->SetShowAssistantButton(true);
  }
};

// Run search box view assistant button tests with and without productivity
// launcher enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         SearchBoxViewAssistantButtonTest,
                         testing::Bool());

// Tests that the assistant button is visible by default.
TEST_P(SearchBoxViewAssistantButtonTest, AssistantButtonVisibleByDefault) {
  EXPECT_TRUE(view()->assistant_button()->GetVisible());
}

// Tests that the assistant button is invisible after typing in the search box,
// and comes back when search box is empty.
TEST_P(SearchBoxViewAssistantButtonTest,
       AssistantButtonChangeVisibilityWithTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_FALSE(view()->assistant_button()->GetVisible());

  // Assistant button is not showing up under zero state.
  KeyPress(ui::VKEY_BACK);
  EXPECT_EQ(IsProductivityLauncherEnabled(),
            view()->assistant_button()->GetVisible());
}

class SearchBoxViewAutocompleteTest : public SearchBoxViewTest {
 public:
  SearchBoxViewAutocompleteTest() = default;
  SearchBoxViewAutocompleteTest(const SearchBoxViewAutocompleteTest&) = delete;
  SearchBoxViewAutocompleteTest& operator=(
      const SearchBoxViewAutocompleteTest&) = delete;
  ~SearchBoxViewAutocompleteTest() override = default;

  void ProcessAutocomplete() {
    view()->ProcessAutocomplete(GetFirstResultView());
  }

  // Sets up the test by creating a SearchResult and displaying an autocomplete
  // suggestion.
  void SetupAutocompleteBehaviorTest() {
    // Send H, E to the SearchBoxView textfield, then trigger an autocomplete.
    KeyPress(ui::VKEY_H);
    KeyPress(ui::VKEY_E);
    // Add a search result with a non-empty title field.
    CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0,
                       u"hello world!", std::u16string());
    base::RunLoop().RunUntilIdle();
    ProcessAutocomplete();
  }
};

// Run search box view autocomplete tests with and without productivity launcher
// enabled.
INSTANTIATE_TEST_SUITE_P(All, SearchBoxViewAutocompleteTest, testing::Bool());

// Tests that autocomplete suggestions are consistent with top SearchResult list
// titles.
TEST_P(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultTitle) {
  // Search result tile tests are not relevant for productivity launcher.
  if (IsProductivityLauncherEnabled())
    return;

  SimulateQuery(u"he");

  // Add two SearchResults, one tile and one list result. Initialize their title
  // field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello list",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 0.5, u"hello tile",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello tile");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo tile");
}

// Tests that autocomplete suggestions are consistent with top SearchResult tile
// titles.
TEST_P(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopTileResultTitle) {
  // Search result tile tests are not relevant for productivity launcher.
  if (IsProductivityLauncherEnabled())
    return;

  SimulateQuery(u"he");

  // Add two SearchResults, one tile and one list result. Initialize their title
  // field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 1.0, u"hello tile",
                     std::u16string());
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"hello list",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello tile");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo tile");
}

// Tests that autocomplete suggestions are consistent with top SearchResult list
// details.
TEST_P(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultDetails) {
  // Search result tile tests are not relevant for productivity launcher.
  if (IsProductivityLauncherEnabled())
    return;

  SimulateQuery(u"he");

  // Add two SearchResults, one tile and one list result. The tile should
  // display first, despite having a lower score. Initialize their details field
  // to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, std::u16string(),
                     u"hello list");
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 0.5, std::u16string(),
                     u"hello tile");
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello tile");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo tile");
}

// Tests that autocomplete suggestions are consistent with top SearchResult tile
// details.
TEST_P(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopTileResultDetails) {
  // Search result tile tests are not relevant for productivity launcher.
  if (IsProductivityLauncherEnabled())
    return;

  SimulateQuery(u"he");

  // Add two SearchResults, one tile and one list result. Initialize their
  // details field to a non-empty string.
  CreateSearchResult(ash::SearchResultDisplayType::kTile, 1.0, std::u16string(),
                     u"hello tile");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, std::u16string(),
                     u"hello list");
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello tile");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo tile");
}

// Tests that SearchBoxView's textfield text does not autocomplete if the top
// result title or details do not have a matching prefix.
TEST_P(SearchBoxViewAutocompleteTest,
       SearchBoxDoesNotAutocompleteWrongCharacter) {
  // Send Z to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_Z);
  // Add a search result with non-empty details and title fields.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"title",
                     u"details");
  base::RunLoop().RunUntilIdle();
  ProcessAutocomplete();
  // The text should not be autocompleted.
  EXPECT_EQ(view()->search_box()->GetText(), u"z");
}

// Tests that autocomplete suggestion will remain if next key in the suggestion
// is typed.
TEST_P(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesAcceptsNextChar) {
  SimulateQuery(u"he");
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello world!",
                     std::u16string());
  base::RunLoop().RunUntilIdle();
  ProcessAutocomplete();

  // After typing L, the highlighted text will be replaced by L.
  KeyPress(ui::VKEY_L);
  std::u16string selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), u"hel");
  EXPECT_EQ(u"", selected_text);

  // After handling autocomplete, the highlighted text will show again.
  ProcessAutocomplete();
  selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello world!");
  EXPECT_EQ(u"lo world!", selected_text);
}

// Tests that autocomplete suggestion is accepted and displayed in SearchModel
// after clicking or tapping on the search box.
TEST_P(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForClick) {
  SetupAutocompleteBehaviorTest();

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  // Forward |mouse_event| to HandleMouseEvent() directly because we cannot
  // test MouseEvents properly due to not having ash dependencies. Static cast
  // to TextfieldController because HandleGestureEvent() is a private method
  // in SearchBoxView. TODO(crbug.com/878984): Derive SearchBoxViewTest from
  // AshTestBase in order to test events using EventGenerator instead.
  static_cast<views::TextfieldController*>(view())->HandleMouseEvent(
      view()->search_box(), mouse_event);
  // Search box autocomplete suggestion is accepted, but it should not
  // trigger another query, thus it is not reflected in Search Model.
  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"he", view()->current_query());
}

TEST_P(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForTap) {
  SetupAutocompleteBehaviorTest();

  ui::GestureEvent gesture_event(0, 0, 0, ui::EventTimeForNow(),
                                 ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  // Forward |gesture_event| to HandleGestureEvent() directly because we
  // cannot test GestureEvents properly due to not having ash dependencies.
  // Static cast to TextfieldController because HandleGestureEvent() is
  // private in SearchBoxView. TODO(crbug.com/878984): Derive
  // SearchBoxViewTest from AshTestBase in order to test events using
  // EventGenerator instead.
  static_cast<views::TextfieldController*>(view())->HandleGestureEvent(
      view()->search_box(), gesture_event);
  // Search box autocomplete suggestion is accepted, but it should not
  // trigger another query, thus it is not reflected in Search Model.
  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"he", view()->current_query());
}

// Tests that autocomplete is not handled if IME is using composition text.
TEST_P(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesNotHandledForIME) {
  // Simulate uncomposited text. The autocomplete should be handled.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->set_highlight_range_for_test(gfx::Range(2, 2));
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello world!",
                     std::u16string());
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();

  std::u16string selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello world!");
  EXPECT_EQ(u"llo world!", selected_text);
  view()->search_box()->SetText(std::u16string());

  // Simulate IME composition text. The autocomplete should not be handled.
  ui::CompositionText composition_text;
  composition_text.text = u"he";
  view()->search_box()->SetCompositionText(composition_text);
  view()->set_highlight_range_for_test(gfx::Range(2, 2));
  ProcessAutocomplete();

  selected_text = view()->search_box()->GetSelectedText();
  EXPECT_EQ(view()->search_box()->GetText(), u"he");
  EXPECT_EQ(u"", selected_text);
}

// TODO(crbug.com/1216082): Refactor the above tests to use AshTestBase, then
// parameterize them based on the ProductivityLauncher flag.
class SearchBoxViewAppListBubbleTest : public AshTestBase {
 public:
  SearchBoxViewAppListBubbleTest() {
    scoped_features_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~SearchBoxViewAppListBubbleTest() override = default;

  static void AddSearchResult(const std::string& id,
                              const std::u16string& title) {
    SearchModel::SearchResults* search_results = GetSearchModel()->results();
    auto search_result = std::make_unique<TestSearchResult>();
    search_result->set_result_id(id);
    search_result->set_display_type(SearchResultDisplayType::kList);
    search_result->SetTitle(title);
    search_result->set_best_match(true);
    search_results->Add(std::move(search_result));
  }

  static void AddAnswerCardResult(const std::string& id,
                                  const std::u16string& title) {
    SearchModel::SearchResults* search_results = GetSearchModel()->results();
    auto search_result = std::make_unique<TestSearchResult>();
    search_result->set_result_id(id);
    search_result->set_display_type(SearchResultDisplayType::kAnswerCard);
    search_result->SetTitle(title);
    search_results->Add(std::move(search_result));
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(SearchBoxViewAppListBubbleTest, AutocompleteCategoricalResult) {
  GetAppListTestHelper()->ShowAppList();

  // Type "he".
  PressAndReleaseKey(ui::VKEY_H);
  PressAndReleaseKey(ui::VKEY_E);

  // Simulate "hello" being returned as a search result.
  AddSearchResult("id", u"hello");
  AddSearchResult("id", u"world");
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.

  // The text autocompletes to "hello" and selects "llo".
  SearchBoxView* view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_EQ(view->search_box()->GetText(), u"hello");
  EXPECT_EQ(view->search_box()->GetSelectedText(), u"llo");

  GetSearchModel()->DeleteAllResults();
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.
  EXPECT_EQ(view->search_box()->GetText(), u"he");
  EXPECT_EQ(view->search_box()->GetSelectedText(), u"");
}

TEST_F(SearchBoxViewAppListBubbleTest, DoNotAutocompleteWithMidQueryCursor) {
  GetAppListTestHelper()->ShowAppList();

  // Type "calculao".
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_L);
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_L);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_O);

  // Simulate "calculator" being returned as a search result.
  AddSearchResult("id", u"calculator");
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.

  // The search box does not autocomplete.
  SearchBoxView* view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_EQ(view->search_box()->GetText(), u"calculao");
  EXPECT_EQ(view->search_box()->GetSelectedText(), u"");

  PressAndReleaseKey(ui::VKEY_LEFT);
  PressAndReleaseKey(ui::VKEY_T);

  GetSearchModel()->DeleteAllResults();
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.
  AddSearchResult("id", u"calculator");
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.

  // The search box does not autocomplete.
  EXPECT_EQ(view->search_box()->GetText(), u"calculato");
  EXPECT_EQ(view->search_box()->GetSelectedText(), u"");
}

TEST_F(SearchBoxViewAppListBubbleTest, ResultSelection) {
  GetAppListTestHelper()->ShowAppList();
  SearchBoxView* view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  ResultSelectionController* controller =
      view->result_selection_controller_for_test();

  // Type "t".
  PressAndReleaseKey(ui::VKEY_T);

  // Simulate two results.
  AddSearchResult("id1", u"title1");
  AddSearchResult("id2", u"title2");
  base::RunLoop().RunUntilIdle();  // Allow observer tasks to run.

  // By default the first item is selected.
  SearchResult* result1 = controller->selected_result()->result();
  ASSERT_TRUE(result1);
  EXPECT_EQ(u"title1", result1->title());

  // Move down one step.
  PressAndReleaseKey(ui::VKEY_DOWN);

  // Second item is selected.
  SearchResult* result2 = controller->selected_result()->result();
  ASSERT_TRUE(result2);
  EXPECT_EQ(u"title2", result2->title());
}

TEST_F(SearchBoxViewAppListBubbleTest, HasAccessibilityHintWhenActive) {
  GetAppListTestHelper()->ShowAppList();
  SearchBoxView* view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_TRUE(view->is_search_box_active());

  EXPECT_TRUE(IsValidSearchBoxAccessibilityHint(
      view->search_box()->GetAccessibleName()));
}

class SearchBoxViewAnimationTest : public AshTestBase {
 public:
  SearchBoxViewAnimationTest() {
    scoped_features_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~SearchBoxViewAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    non_zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    GetSearchModel()->search_box()->SetShowAssistantButton(true);
  }

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;
  base::test::ScopedFeatureList scoped_features_;
};

// Test that the search box image buttons fade in and out correctly when the
// search box is activated and deactivated.
TEST_F(SearchBoxViewAnimationTest, SearchBoxImageButtonAnimations) {
  auto* search_box = GetAppListTestHelper()->GetSearchBoxView();

  // Initially the assistant button should be shown, and the close button
  // hidden.
  EXPECT_FALSE(search_box->close_button()->GetVisible());
  EXPECT_TRUE(search_box->assistant_button()->GetVisible());

  // Set search box to active state.
  search_box->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // Close button should be fading in.
  EXPECT_TRUE(search_box->close_button()->GetVisible());
  auto* close_animator = search_box->close_button()->layer()->GetAnimator();
  ASSERT_TRUE(close_animator);
  EXPECT_TRUE(close_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(close_animator->GetTargetOpacity(), 1.0f);

  // Assistant button should be fading out.
  EXPECT_TRUE(search_box->assistant_button()->GetVisible());
  auto* assistant_animator =
      search_box->assistant_button()->layer()->GetAnimator();
  EXPECT_TRUE(assistant_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(assistant_animator->GetTargetOpacity(), 0.0f);

  // Set search box to inactive state, hiding the close button.
  search_box->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);

  // Close button should be fading out.
  EXPECT_TRUE(search_box->close_button()->GetVisible());
  EXPECT_TRUE(close_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(close_animator->GetTargetOpacity(), 0.0f);

  // Assistant button should be fading in.
  EXPECT_TRUE(search_box->assistant_button()->GetVisible());
  ASSERT_TRUE(assistant_animator);
  EXPECT_TRUE(assistant_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(assistant_animator->GetTargetOpacity(), 1.0f);
}

// Test that activating and deactivating the search box causes the search icon
// to animate.
TEST_F(SearchBoxViewAnimationTest, SearchBoxIconImageViewAnimation) {
  auto* search_box = GetAppListTestHelper()->GetSearchBoxView();

  // Keep track of the animator for the icon layer which will animate out.
  auto* old_animator = search_box->search_icon()->layer()->GetAnimator();

  // Set search box to active state.
  search_box->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // Check that the old layer is fading out and the new animator is fading in.
  auto* animator = search_box->search_icon()->layer()->GetAnimator();
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(animator->GetTargetOpacity(), 1.0f);
  EXPECT_TRUE(old_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(old_animator->GetTargetOpacity(), 0.0f);

  // Set search box to inactive state.
  search_box->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);

  old_animator = animator;
  animator = search_box->search_icon()->layer()->GetAnimator();

  // Check that the old layer is fading out and the new layer is fading in.
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(animator->GetTargetOpacity(), 1.0f);
  EXPECT_TRUE(old_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(old_animator->GetTargetOpacity(), 0.0f);
}

}  // namespace
}  // namespace ash
