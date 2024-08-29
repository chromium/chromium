// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view_delegate.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_mixer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"

namespace {
// kBestMatch is the second result container for productivity launcher search.
constexpr int kBestMatchIndex = 1;

// Copied from ash/app_list/views/app_list_search_view.cc
constexpr base::TimeDelta kNotifyA11yDelay = base::Milliseconds(1500);

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
          l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS)),
      l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_IMAGES))};
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
    if (!absl::ascii_isalnum(key_event.key_code())) {
      ++count_;
      return true;
    }
    return false;
  }
  int count_;
};

class SearchBoxViewTest : public views::test::WidgetTest,
                          public SearchBoxViewDelegate {
 public:
  SearchBoxViewTest()
      : views::test::WidgetTest(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

  SearchBoxViewTest(const SearchBoxViewTest&) = delete;
  SearchBoxViewTest& operator=(const SearchBoxViewTest&) = delete;

  ~SearchBoxViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::test::WidgetTest::SetUp();

    // Tests have an implicit dependency on the color providers.
    ui::ColorProviderManager::Get().AppendColorProviderInitializer(
        base::BindRepeating(AddCrosStylesColorMixer));
    ui::ColorProviderManager::Get().AppendColorProviderInitializer(
        base::BindRepeating(AddAshColorMixer));

    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));

    std::unique_ptr<SearchBoxView> view;
    // Initialize SearchBoxView like clamshell productivity launcher.
    view = std::make_unique<SearchBoxView>(this, &view_delegate_,
                                           /*is_bubble_app_list=*/true);
    view->InitializeForBubbleLauncher();
    view_ = widget_->GetContentsView()->AddChildView(std::move(view));

    search_view_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<AppListSearchView>(
            &view_delegate_, /*dialog_controller=*/nullptr, view_));
    widget_->Show();
  }

  void TearDown() override {
    ui::ColorProviderManager::ResetForTesting();
    if (app_list_view_) {
      app_list_view_->GetWidget()->Close();
    }
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

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

  void KeyPress(ui::KeyboardCode key_code, bool is_shift_down = false) {
    ui::KeyEvent event(ui::EventType::kKeyPressed, key_code,
                       is_shift_down ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    view()->search_box()->OnKeyEvent(&event);
    // Emulates the input method.
    if (absl::ascii_isalnum(key_code)) {
      char16_t character = absl::ascii_tolower(key_code);
      view()->search_box()->InsertText(
          std::u16string(1, character),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    }
  }

  // Creates a SearchResult with the given parameters.
  void CreateSearchResult(ash::SearchResultDisplayType display_type,
                          double display_score,
                          const std::u16string& title,
                          const std::u16string& details,
                          const ash::AppListSearchResultCategory& category) {
    CreateSearchResultAt(results()->item_count(), display_type, display_score,
                         title, details, category);
  }

  // Creates a SearchResult with the given parameters at the given index in
  // the results list.
  void CreateSearchResultAt(size_t index,
                            ash::SearchResultDisplayType display_type,
                            double display_score,
                            const std::u16string& title,
                            const std::u16string& details,
                            const ash::AppListSearchResultCategory& category) {
    auto search_result = std::make_unique<TestSearchResult>();
    search_result->set_result_id(base::NumberToString(++last_result_id_));
    search_result->set_display_type(display_type);
    search_result->set_display_score(display_score);
    search_result->SetTitle(title);
    search_result->SetDetails(details);
    search_result->SetCategory(category);
    search_result->set_best_match(true);
    results()->AddAt(index, std::move(search_result));
  }

  SearchModel::SearchResults* results() { return GetSearchModel()->results(); }

  SearchResultBaseView* GetFirstResultView() {
    return search_view_->result_container_views_for_test()[kBestMatchIndex]
        ->GetFirstResultView();
  }

  SearchResultBaseView* GetResultViewAt(size_t index) {
    return search_view_->result_container_views_for_test()[kBestMatchIndex]
        ->GetResultViewAt(index);
  }

  ResultSelectionController* GetResultSelectionController() {
    return search_view_->result_selection_controller_for_test();
  }

  void OnSearchResultContainerResultsChanged() {
    search_view_->OnSearchResultContainerResultsChanged();
  }

  void SimulateQuery(const std::u16string& query) {
    view()->search_box()->InsertText(
        query,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(const std::u16string& trimmed_query,
                    bool initiated_by_user) override {
    search_view_->UpdateForNewSearch(!trimmed_query.empty());
  }
  void AssistantButtonPressed() override {}
  void CloseButtonPressed() override {}
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override {}
  bool CanSelectSearchResults() override { return true; }

  base::test::ScopedFeatureList scoped_feature_list_;
  AshColorProvider ash_color_provider_;
  raw_ptr<AppListSearchView, DanglingUntriaged> search_view_ = nullptr;
  AppListTestViewDelegate view_delegate_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
  raw_ptr<AppListView> app_list_view_ = nullptr;
  raw_ptr<SearchBoxView, DanglingUntriaged> view_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<KeyPressCounterView> counter_view_ =
      nullptr;  // Owned by views hierarchy.
  int last_result_id_ = 0;
};

TEST_F(SearchBoxViewTest, SearchBoxTextUsesAppListSearchBoxTextColor) {
  // With darklight mode enabled by default, search box text color should be the
  // same with and without productivity launcher enabled.
  EXPECT_EQ(view()->search_box()->GetTextColor(),
            view()->GetColorProvider()->GetColor(kColorAshTextColorPrimary));
}

// Tests that the close button is invisible by default.
TEST_F(SearchBoxViewTest, CloseButtonInvisibleByDefault) {
  EXPECT_FALSE(view()->filter_and_close_button_container()->GetVisible());
}

// Tests that the close button becomes visible after typing in the search box.
TEST_F(SearchBoxViewTest, CloseButtonVisibleAfterTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_TRUE(view()->filter_and_close_button_container()->GetVisible());
}

// Tests that the filter button is not created if the image search feature is
// disabled.
TEST_F(SearchBoxViewTest, FilterButtonNotCreatedWithDisabledImageSearch) {
  ASSERT_FALSE(features::IsProductivityLauncherImageSearchEnabled());
  EXPECT_FALSE(view()->filter_button());

  // The filter button is still not created after typing in the search box.
  KeyPress(ui::VKEY_A);
  EXPECT_FALSE(view()->filter_button());
}

// Tests that the close button is still visible after the search box is
// activated (in zero state).
TEST_F(SearchBoxViewTest, CloseButtonVisibleInZeroStateSearchBox) {
  SetSearchBoxActive(true, ui::EventType::kMousePressed);
  EXPECT_FALSE(view()->filter_and_close_button_container()->GetVisible());
}

// TODO(crbug.com/40913066): Re-enable this test
TEST_F(SearchBoxViewTest,
       DISABLED_AccessibilityHintRemovedWhenSearchBoxActive) {
  EXPECT_TRUE(IsValidSearchBoxAccessibilityHint(
      view()->search_box()->GetViewAccessibility().GetCachedName()));
  SetSearchBoxActive(true, ui::EventType::kMousePressed);
  EXPECT_TRUE(IsValidSearchBoxAccessibilityHint(
      view()->search_box()->GetViewAccessibility().GetCachedName()));
}

// Tests that the black Google icon is used for an inactive Google search.
TEST_F(SearchBoxViewTest, SearchBoxInactiveSearchBoxGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(false, ui::EventType::kUnknown);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kGoogleBlackIcon, view()->GetSearchBoxIconSize(),
      view()->GetColorProvider()->GetColor(kColorAshButtonIconColor));

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the colored Google icon is used for an active Google search.
TEST_F(SearchBoxViewTest, SearchBoxActiveSearchEngineGoogle) {
  SetSearchEngineIsGoogle(true);
  SetSearchBoxActive(true, ui::EventType::kMousePressed);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      vector_icons::kGoogleColorIcon, view()->GetSearchBoxIconSize(),
      view()->GetColorProvider()->GetColor(kColorAshButtonIconColor));

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an inactive non-Google search.
TEST_F(SearchBoxViewTest, SearchBoxInactiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(false, ui::EventType::kUnknown);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, view()->GetSearchBoxIconSize(),
      view()->GetColorProvider()->GetColor(kColorAshButtonIconColor));

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that the non-Google icon is used for an active non-Google search.
TEST_F(SearchBoxViewTest, SearchBoxActiveSearchEngineNotGoogle) {
  SetSearchEngineIsGoogle(false);
  SetSearchBoxActive(true, ui::EventType::kUnknown);
  const gfx::ImageSkia expected_icon = gfx::CreateVectorIcon(
      kSearchEngineNotGoogleIcon, view()->GetSearchBoxIconSize(),
      view()->GetColorProvider()->GetColor(kColorAshButtonIconColor));

  const gfx::ImageSkia actual_icon = view()->search_icon()->GetImage();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*expected_icon.bitmap(),
                                         *actual_icon.bitmap()));
}

// Tests that traversing search results is disabled while results are being
// updated.
TEST_F(SearchBoxViewTest, ChangeSelectionWhileResultsAreChanging) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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
                       std::u16string(),
                       ash::AppListSearchResultCategory::kWeb);
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
TEST_F(SearchBoxViewTest, ChangeSelectionWhileResultsAreBeingRemoved) {
  SimulateQuery(u"test");

  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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

TEST_F(SearchBoxViewTest, UserSelectionNotOverridenByNewResults) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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
                       std::u16string(),
                       ash::AppListSearchResultCategory::kWeb);
  // Finish results update.
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     u"testing almost", std::u16string(),
                     ash::AppListSearchResultCategory::kWeb);
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
                       std::u16string(),
                       ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"test", selection->result()->title());
}

TEST_F(SearchBoxViewTest,
       UserSelectionInNonDefaultContainerNotOverridenByNewResults) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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

  // Add a new result at the end, and verify the selection stays the same.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.2,
                     u"testing almost", std::u16string(),
                     ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Remove the result before the selected one, and verify the selection remains
  // the same.
  results()->RemoveAt(0);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"testing", selection->result()->title());

  // Result should be reset if the selected result is removed.
  results()->RemoveAt(0);
  base::RunLoop().RunUntilIdle();

  // Tile results are not created when testing productivity launcher.
  selection = GetResultSelectionController()->selected_result();

  EXPECT_EQ(u"testing almost", selection->result()->title());

  // New result can override the default selection.
  CreateSearchResultAt(0, ash::SearchResultDisplayType::kList, 1.0, u"test",
                       std::u16string(),
                       ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();

  selection = GetResultSelectionController()->selected_result();
  EXPECT_EQ(u"test", selection->result()->title());
}

// Tests that the default selection is reset after resetting and reactivating
// the search box.
TEST_F(SearchBoxViewTest, ResetSelectionAfterResettingSearchBox) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"test1",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"test2",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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
  SetSearchBoxActive(true, ui::EventType::kUnknown);
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

// Tests that changing selection in the search box results updates the active
// descendent id in the search box textfield.
TEST_F(SearchBoxViewTest, SearchTextfieldAccessibleActiveDescendantId) {
  ui::AXNodeData data_textfield;
  auto* textfield = view()->search_box();
  base::test::TaskEnvironment* task_environment_ = task_environment();
  textfield->GetViewAccessibility().GetAccessibleNodeData(&data_textfield);
  EXPECT_FALSE(data_textfield.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));

  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"testing",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();  // Finish search results update.
  task_environment_->FastForwardBy(
      kNotifyA11yDelay);  // Advance time to trigger a11y notification.

  // First result selected by default.
  data_textfield = ui::AXNodeData();
  textfield->GetViewAccessibility().GetAccessibleNodeData(&data_textfield);
  EXPECT_TRUE(data_textfield.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));
  EXPECT_EQ(GetResultViewAt(0)->GetViewAccessibility().GetUniqueId(),
            data_textfield.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId));

  // Move down to select the second result.
  KeyPress(ui::VKEY_DOWN);
  base::RunLoop().RunUntilIdle();
  task_environment_->FastForwardBy(kNotifyA11yDelay);
  data_textfield = ui::AXNodeData();
  textfield->GetViewAccessibility().GetAccessibleNodeData(&data_textfield);
  EXPECT_EQ(GetResultViewAt(1)->GetViewAccessibility().GetUniqueId(),
            data_textfield.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId));

  // Removing selected result resets the selection to default.
  results()->RemoveAt(1);
  base::RunLoop().RunUntilIdle();
  task_environment_->FastForwardBy(kNotifyA11yDelay);
  data_textfield = ui::AXNodeData();
  textfield->GetViewAccessibility().GetAccessibleNodeData(&data_textfield);
  EXPECT_EQ(GetResultViewAt(0)->GetViewAccessibility().GetUniqueId(),
            data_textfield.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId));

  // Clear search results.
  results()->RemoveAt(0);
  base::RunLoop().RunUntilIdle();
  task_environment_->FastForwardBy(kNotifyA11yDelay);
  data_textfield = ui::AXNodeData();
  textfield->GetViewAccessibility().GetAccessibleNodeData(&data_textfield);
  EXPECT_FALSE(data_textfield.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));
}

TEST_F(SearchBoxViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kTextField, data.role);
}

TEST_F(SearchBoxViewTest, SearchResultBaseViewAccessibleProperties) {
  SimulateQuery(u"test");
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.7, u"tester",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();
  auto* result_base_view = GetFirstResultView();
  ui::AXNodeData data;

  ASSERT_TRUE(result_base_view);
  result_base_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kClick, data.GetDefaultActionVerb());

  result_base_view->SetEnabled(false);
  data = ui::AXNodeData();
  result_base_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kClick, data.GetDefaultActionVerb());

  result_base_view->SetVisible(false);
  data = ui::AXNodeData();
  result_base_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  result_base_view->SetVisible(true);
  data = ui::AXNodeData();
  result_base_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kClick, data.GetDefaultActionVerb());
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

// Tests that the assistant button is visible by default.
TEST_F(SearchBoxViewAssistantButtonTest, AssistantButtonVisibleByDefault) {
  EXPECT_TRUE(view()->edge_button_container()->GetVisible());
  EXPECT_TRUE(view()->assistant_button()->GetVisible());
}

// Tests that the assistant button is invisible after typing in the search box,
// and comes back when search box is empty.
TEST_F(SearchBoxViewAssistantButtonTest,
       AssistantButtonChangeVisibilityWithTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_FALSE(view()->edge_button_container()->GetVisible());

  KeyPress(ui::VKEY_BACK);
  EXPECT_TRUE(view()->edge_button_container()->GetVisible());
  EXPECT_TRUE(view()->assistant_button()->GetVisible());
}

class SearchBoxViewFilterButtonTest : public SearchBoxViewTest {
 public:
  SearchBoxViewFilterButtonTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kJelly, features::kLauncherSearchControl,
         features::kFeatureManagementLocalImageSearch},
        {});
  }
  SearchBoxViewFilterButtonTest(const SearchBoxViewFilterButtonTest&) = delete;
  SearchBoxViewFilterButtonTest& operator=(
      const SearchBoxViewFilterButtonTest&) = delete;
  ~SearchBoxViewFilterButtonTest() override = default;
};

// Tests that the filter button is invisible by default.
TEST_F(SearchBoxViewFilterButtonTest, FilterButtonInvisibleByDefault) {
  EXPECT_FALSE(view()->filter_button()->parent()->GetVisible());
}

// Tests that the filter button becomes visible after typing in the search box.
TEST_F(SearchBoxViewFilterButtonTest, FilterButtonVisibleAfterTyping) {
  KeyPress(ui::VKEY_A);
  EXPECT_TRUE(view()->filter_button()->parent()->GetVisible());
}

class SearchBoxViewAutocompleteTest : public SearchBoxViewTest {
 public:
  SearchBoxViewAutocompleteTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }
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
                       u"hello world!", std::u16string(),
                       ash::AppListSearchResultCategory::kWeb);
    base::RunLoop().RunUntilIdle();
    ProcessAutocomplete();
  }
};

// Tests that autocomplete suggestions are consistent with top SearchResult list
// titles.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultTitle) {
  SimulateQuery(u"he");

  // Add two SearchResults. The higher ranked result should be selected by
  // default and it's title should be autocompleted into the search box.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 2.0, u"hello list",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello list2",
                     std::u16string(), ash::AppListSearchResultCategory::kApps);
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello list");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo list");

  EXPECT_EQ("Websites", view()->GetSearchBoxGhostTextForTest());
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ("Apps", view()->GetSearchBoxGhostTextForTest());
}

// Tests that autocomplete suggestions are consistent with top SearchResult list
// details.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxAutocompletesTopListResultDetails) {
  SimulateQuery(u"he");

  // Add two SearchResults. The higher ranked result should be selected by
  // default and it's title should be autocompleted into the search box.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 2.0, std::u16string(),
                     u"hello list", ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, std::u16string(),
                     u"hello list2", ash::AppListSearchResultCategory::kApps);
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();
  EXPECT_EQ(view()->search_box()->GetText(), u"hello list");
  EXPECT_EQ(view()->search_box()->GetSelectedText(), u"llo list");

  EXPECT_EQ("Websites", view()->GetSearchBoxGhostTextForTest());
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ("Apps", view()->GetSearchBoxGhostTextForTest());
}

// Tests that SearchBoxView's textfield text does not autocomplete if the top
// result title or details do not have a matching prefix.
TEST_F(SearchBoxViewAutocompleteTest,
       SearchBoxDoesNotAutocompleteWrongCharacter) {
  // Send ABC to the SearchBoxView textfield, then trigger an autocomplete.
  KeyPress(ui::VKEY_A);
  KeyPress(ui::VKEY_B);
  KeyPress(ui::VKEY_C);
  // Add a search result with non-empty details and title fields.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"title",
                     u"details", ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();
  ProcessAutocomplete();
  // The text should not be autocompleted.
  EXPECT_EQ(view()->search_box()->GetText(), u"abc");

  EXPECT_EQ("title - Websites", view()->GetSearchBoxGhostTextForTest());
}

// Tests that autocomplete suggestion will remain if next key in the suggestion
// is typed.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesAcceptsNextChar) {
  SimulateQuery(u"he");
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello world!",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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

  EXPECT_EQ("Websites", view()->GetSearchBoxGhostTextForTest());
}

// Tests that autocomplete suggestion is accepted and displayed in SearchModel
// after clicking or tapping on the search box.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForClick) {
  SetupAutocompleteBehaviorTest();

  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  // Forward |mouse_event| to HandleMouseEvent() directly because we cannot
  // test MouseEvents properly due to not having ash dependencies. Static cast
  // to TextfieldController because HandleGestureEvent() is a private method
  // in SearchBoxView. TODO(crbug.com/41410759): Derive SearchBoxViewTest from
  // AshTestBase in order to test events using EventGenerator instead.
  static_cast<views::TextfieldController*>(view())->HandleMouseEvent(
      view()->search_box(), mouse_event);
  // Search box autocomplete suggestion is accepted, and triggers another query.
  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"hello world!", view()->current_query());
  EXPECT_EQ(u"", view()->search_box()->GetSelectedText());
  EXPECT_EQ("", view()->GetSearchBoxGhostTextForTest());
}

TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForTap) {
  SetupAutocompleteBehaviorTest();

  ui::GestureEvent gesture_event(
      0, 0, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  // Forward |gesture_event| to HandleGestureEvent() directly because we
  // cannot test GestureEvents properly due to not having ash dependencies.
  // Static cast to TextfieldController because HandleGestureEvent() is
  // private in SearchBoxView. TODO(crbug.com/41410759): Derive
  // SearchBoxViewTest from AshTestBase in order to test events using
  // EventGenerator instead.
  static_cast<views::TextfieldController*>(view())->HandleGestureEvent(
      view()->search_box(), gesture_event);
  // Search box autocomplete suggestion is accepted, and trigger updated query.
  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"hello world!", view()->current_query());
  EXPECT_EQ(u"", view()->search_box()->GetSelectedText());
  EXPECT_EQ("", view()->GetSearchBoxGhostTextForTest());
}

TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAcceptsAutocompleteForRightKey) {
  SetupAutocompleteBehaviorTest();

  KeyPress(ui::VKEY_RIGHT);

  // Search box autocomplete suggestion is accepted, and trigger updated query.
  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"hello world!", view()->current_query());
  EXPECT_EQ(u"", view()->search_box()->GetSelectedText());
  EXPECT_EQ("", view()->GetSearchBoxGhostTextForTest());

  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5, u"hello world 1",
                     std::u16string(), ash::AppListSearchResultCategory::kApps);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 0.5,
                     u"hello world! 123", std::u16string(),
                     ash::AppListSearchResultCategory::kWeb);
  base::RunLoop().RunUntilIdle();

  // Change selection to the non-default item, and verify the search box text is
  // updated as expected.
  KeyPress(ui::VKEY_DOWN);

  EXPECT_EQ(u"hello world!", view()->search_box()->GetText());
  EXPECT_EQ(u"hello world!", view()->current_query());
  EXPECT_EQ(u"", view()->search_box()->GetSelectedText());
  EXPECT_EQ("hello world 1 - Apps", view()->GetSearchBoxGhostTextForTest());

  KeyPress(ui::VKEY_DOWN);

  EXPECT_EQ(u"hello world! 123", view()->search_box()->GetText());
  EXPECT_EQ(u"hello world!", view()->current_query());
  EXPECT_EQ(u" 123", view()->search_box()->GetSelectedText());
  EXPECT_EQ("Websites", view()->GetSearchBoxGhostTextForTest());
}

// Tests that autocomplete is not handled if IME is using composition text.
TEST_F(SearchBoxViewAutocompleteTest, SearchBoxAutocompletesNotHandledForIME) {
  // Simulate uncomposited text. The autocomplete should be handled.
  KeyPress(ui::VKEY_H);
  KeyPress(ui::VKEY_E);
  view()->set_highlight_range_for_test(gfx::Range(2, 2));
  // Add a search result with a non-empty title field.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello world!",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
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

  EXPECT_EQ("", view()->GetSearchBoxGhostTextForTest());
}

// TODO(crbug.com/40184650): Refactor the above tests to use AshTestBase.
class SearchBoxViewAppListBubbleTest : public AshTestBase {
 public:
  SearchBoxViewAppListBubbleTest() = default;
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
      view->search_box()->GetViewAccessibility().GetCachedName()));
}

class SearchBoxViewTabletTest : public AshTestBase {
 public:
  SearchBoxViewTabletTest() = default;
  ~SearchBoxViewTabletTest() override = default;
  void SetUp() override {
    AshTestBase::SetUp();
    ash::TabletModeControllerTestApi().EnterTabletMode();
  }
};

// Tests that the search box is inactive by default.
TEST_F(SearchBoxViewTabletTest, SearchBoxInactiveByDefault) {
  ASSERT_FALSE(
      GetAppListTestHelper()->GetSearchBoxView()->is_search_box_active());
}

class SearchBoxViewAnimationTest : public AshTestBase {
 public:
  SearchBoxViewAnimationTest() = default;
  ~SearchBoxViewAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ash::TabletModeControllerTestApi().EnterTabletMode();
    non_zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    GetSearchModel()->search_box()->SetShowAssistantButton(true);
  }

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;
};

// Test that the search box image buttons fade in and out correctly when the
// search box is activated and deactivated.
TEST_F(SearchBoxViewAnimationTest, SearchBoxImageButtonAnimations) {
  auto* search_box = GetAppListTestHelper()->GetSearchBoxView();

  // Initially the assistant button should be shown, and the close button
  // hidden.
  EXPECT_FALSE(search_box->filter_and_close_button_container()->GetVisible());
  EXPECT_TRUE(search_box->edge_button_container()->GetVisible());
  EXPECT_TRUE(search_box->assistant_button()->GetVisible());

  // Set search box to active state.
  search_box->SetSearchBoxActive(true, ui::EventType::kMousePressed);

  // Close button should be fading in.
  EXPECT_TRUE(search_box->filter_and_close_button_container()->GetVisible());
  auto* close_animator =
      search_box->filter_and_close_button_container()->layer()->GetAnimator();
  ASSERT_TRUE(close_animator);
  EXPECT_TRUE(close_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(close_animator->GetTargetOpacity(), 1.0f);

  // Assistant button should be fading out.
  EXPECT_TRUE(search_box->edge_button_container()->GetVisible());
  EXPECT_TRUE(search_box->assistant_button()->GetVisible());
  auto* assistant_animator =
      search_box->edge_button_container()->layer()->GetAnimator();
  EXPECT_TRUE(assistant_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(assistant_animator->GetTargetOpacity(), 0.0f);

  // Set search box to inactive state, hiding the close button.
  search_box->SetSearchBoxActive(false, ui::EventType::kMousePressed);

  // Close button should be fading out.
  EXPECT_TRUE(search_box->filter_and_close_button_container()->GetVisible());
  EXPECT_TRUE(close_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(close_animator->GetTargetOpacity(), 0.0f);

  // Assistant button should be fading in.
  EXPECT_TRUE(search_box->edge_button_container()->GetVisible());
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
  search_box->SetSearchBoxActive(true, ui::EventType::kMousePressed);

  // Check that the old layer is fading out and the new animator is fading in.
  auto* animator = search_box->search_icon()->layer()->GetAnimator();
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(animator->GetTargetOpacity(), 1.0f);
  EXPECT_TRUE(old_animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_EQ(old_animator->GetTargetOpacity(), 0.0f);

  // Set search box to inactive state.
  search_box->SetSearchBoxActive(false, ui::EventType::kMousePressed);

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

// Accessible value test for the search box.
TEST_F(SearchBoxViewAutocompleteTest, AccessibleValue) {
  SimulateQuery(u"he");

  // Add two SearchResults. The higher ranked result should be selected by
  // default and it's title should be autocompleted into the search box.
  CreateSearchResult(ash::SearchResultDisplayType::kList, 2.0, u"hello list",
                     std::u16string(), ash::AppListSearchResultCategory::kWeb);
  CreateSearchResult(ash::SearchResultDisplayType::kList, 1.0, u"hello list2",
                     std::u16string(), ash::AppListSearchResultCategory::kApps);
  base::RunLoop().RunUntilIdle();

  ProcessAutocomplete();

  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(view()->search_box()->GetText(), u"hello list");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_APP_LIST_SEARCH_BOX_AUTOCOMPLETE,
                                       view()->search_box()->GetText()),
            data.GetString16Attribute(ax::mojom::StringAttribute::kValue));

  EXPECT_EQ("Websites", view()->GetSearchBoxGhostTextForTest());
  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ("Apps", view()->GetSearchBoxGhostTextForTest());

  ui::AXNodeData data2;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data2);
  EXPECT_EQ(view()->search_box()->GetText(), u"hello list2");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_APP_LIST_SEARCH_BOX_AUTOCOMPLETE,
                                       view()->search_box()->GetText()),
            data2.GetString16Attribute(ax::mojom::StringAttribute::kValue));
}

class SunfishLauncherButtonTest : public AshTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  SunfishLauncherButtonTest() {
    if (IsSunfishEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(features::kSunfishFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kSunfishFeature);
    }
  }
  ~SunfishLauncherButtonTest() override = default;

  bool IsSunfishEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, SunfishLauncherButtonTest, testing::Bool());

// Tests the launcher button that may be found in the app list, next to the
// search field.
TEST_P(SunfishLauncherButtonTest, ButtonVisibility) {
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  EXPECT_FALSE(home_button->IsShowingAppList());

  LeftClickOn(home_button);

  ASSERT_TRUE(home_button->IsShowingAppList());
  auto* sunfish_button =
      GetAppListTestHelper()->GetBubbleSearchBoxView()->sunfish_button();
  ASSERT_EQ(IsSunfishEnabled(), !!sunfish_button);

  if (IsSunfishEnabled()) {
    // The app list will contain the sunfish launcher button next to the search
    // field.
    LeftClickOn(sunfish_button);

    auto* session = CaptureModeController::Get()->capture_mode_session();
    ASSERT_TRUE(session);
    ASSERT_EQ(BehaviorType::kSunfish,
              session->active_behavior()->behavior_type());
  }
}

}  // namespace
}  // namespace ash
