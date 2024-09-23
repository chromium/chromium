// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_box_model_observer.h"
#include "ash/ash_export.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/search_box/search_box_view_base.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class MenuItemView;
class Textfield;
class View;
}  // namespace views

namespace ash {

class AppListViewDelegate;
class FilterMenuAdapter;
class ResultSelectionController;
class SearchBoxViewDelegate;
class SearchResultBaseView;
using QueryChangedCallback = base::RepeatingCallback<void()>;

// Subclass of SearchBoxViewBase. SearchBoxModel is its data model
// that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class ASH_EXPORT SearchBoxView : public SearchBoxViewBase,
                                 public AppListModelProvider::Observer,
                                 public SearchBoxModelObserver,
                                 public LauncherSearchIphView::Delegate,
                                 public AssistantViewDelegateObserver {
  METADATA_HEADER(SearchBoxView, SearchBoxViewBase)

 public:
  enum class PlaceholderTextType {
    kShortcuts = 0,
    kTabs = 1,
    kSettings = 2,
    kGames = 3,
    kImages = 4
  };

  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate,
                bool is_app_list_bubble);

  SearchBoxView(const SearchBoxView&) = delete;
  SearchBoxView& operator=(const SearchBoxView&) = delete;

  ~SearchBoxView() override;

  // Initializes the search box style for usage in bubble (clamshell mode)
  // launcher.
  void InitializeForBubbleLauncher();

  // Initializes the search box style for usage in fullscreen (tablet mode)
  // launcher.
  void InitializeForFullscreenLauncher();

  // Must be called before the user interacts with the search box. Cannot be
  // part of Init() because the controller isn't available until after Init()
  // is called.
  void SetResultSelectionController(ResultSelectionController* controller);

  // Resets state of SearchBoxView so it can be reshown.
  void ResetForShow();

  // Returns the total focus ring spacing for use in folders.
  static int GetFocusRingSpacing();

  // Overridden from SearchBoxViewBase:
  void UpdateSearchTextfieldAccessibleActiveDescendantId() override;
  void UpdateKeyboardVisibility() override;
  void HandleQueryChange(const std::u16string& query,
                         bool initiated_by_user) override;
  void UpdatePlaceholderTextStyle() override;
  void UpdateSearchBoxBorder() override;
  void OnSearchBoxActiveChanged(bool active) override;
  void UpdateSearchBoxFocusPaint() override;
  void OnAfterUserAction(views::Textfield* sender) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Overridden from views::View:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnPaintBorder(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void AddedToWidget() override;

  // LauncherSearchIphView::Delegate:
  void RunLauncherSearchQuery(const std::u16string& query) override;
  void OpenAssistantPage() override;

  // AssistantViewDelegateObserver:
  void OnLauncherSearchChipPressed(const std::u16string& query) override;

  // Shows the category filter menu that allows users to enable/disable specific
  // search categories.
  void ShowFilterMenu();

  // Called when the category filter menu is closed.
  void OnFilterMenuClosed();

  // Returns the menu item view in the category filter menu that indicates the
  // `category` button. This should only be called when `filter_button_` exists
  // and the menu is opened.
  views::MenuItemView* GetFilterMenuItemByCategory(
      AppListSearchControlCategory category);

  // Returns true if the category filter menu is opened. This should only be
  // called when `filter_button_` exists.
  bool IsFilterMenuOpen();

  // Updates the search box's background corner radius and color based on the
  // state of AppListModel.
  void UpdateBackground(AppListState target_state);

  // Updates the search box's layout based on the state of AppListModel.
  void UpdateLayout(AppListState target_state, int target_state_height);

  // Returns background border corner radius in the given state.
  int GetSearchBoxBorderCornerRadiusForState(AppListState state) const;

  // Returns background color for the given state.
  SkColor GetBackgroundColorForState(AppListState state) const;

  // Sets the autocomplete text if autocomplete conditions are met.
  void ProcessAutocomplete(SearchResultBaseView* first_result_view);

  // Sets up prefix match autocomplete. Returns true if successful.
  bool ProcessPrefixMatchAutocomplete(SearchResult* search_result,
                                      const std::u16string& user_typed_text);

  // Removes all autocomplete text.
  void ClearAutocompleteText();

  // Updates the search box with |new_query| and starts a new search.
  void UpdateQuery(const std::u16string& new_query);

  // Moves the focus back to search box and find a search result to select.
  void EnterSearchResultSelection(const ui::KeyEvent& event);

  // Clears the search query and de-activate the search box.
  void ClearSearchAndDeactivateSearchBox();

  // Sets the view accessibility ID of the search box's active descendant.
  // The active descendant should be the currently selected result view in the
  // search results list.
  // `nullopt` indicates no active descendant, i.e. that no result is selected.
  void SetA11yActiveDescendant(
      const std::optional<ui::AXPlatformNodeId>& active_descendant);

  // Refreshes the placeholder text with a fixed one rather than the one picked
  // up randomly
  void UseFixedPlaceholderTextForTest();

  ResultSelectionController* result_selection_controller_for_test() {
    return result_selection_controller_;
  }
  void set_highlight_range_for_test(const gfx::Range& range) {
    highlight_range_ = range;
  }

  const std::u16string& current_query() const { return current_query_; }

  // Update search box view background when result container visibility changes.
  void OnResultContainerVisibilityChanged(bool visible);

  // Whether the search box has a non-empty, non-whitespace query.
  bool HasValidQuery();

  // Calculates the correct sizing for search box icons and buttons.
  int GetSearchBoxIconSize();
  int GetSearchBoxButtonSize();

  void SetQueryChangedCallback(QueryChangedCallback callback);

 private:
  class FocusRingLayer;

  // Called when the close button within the search box gets pressed.
  void CloseButtonPressed();

  // Called when the assistant button within the search box gets pressed.
  void AssistantButtonPressed();

  // Called when the sunfish launcher button within the search box gets pressed.
  void SunfishButtonPressed();

  // Updates the icon shown left of the search box texfield.
  void UpdateSearchIcon();

  // Whether 'autocomplete_text' is a valid candidate for classic highlighted
  // autocomplete.
  bool IsValidAutocompleteText(const std::u16string& autocomplete_text);

  // Updates the text field text color.
  void UpdateTextColor();

  // Updates the search box placeholder text and accessible name.
  void UpdatePlaceholderTextAndAccessibleName();

  // Notifies SearchBoxViewDelegate that the autocomplete text is valid.
  void AcceptAutocompleteText();

  // Returns true if there is currently an autocomplete suggestion in
  // search_box().
  bool HasAutocompleteText();

  // After verifying autocomplete text is valid, sets the current searchbox
  // text to the autocomplete text and sets the text highlight.
  void SetAutocompleteText(const std::u16string& autocomplete_text);

  // Returns the text shown in the text field when there is no text inputs.
  SearchBoxView::PlaceholderTextType SelectPlaceholderText() const;

  // Overridden from views::TextfieldController:
  void OnBeforeUserAction(views::Textfield* sender) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  // Updates search_box() for the |selected_result|. Should be called when the
  // selected search result changes.
  void UpdateSearchBoxForSelectedResult(SearchResult* selected_result);

  // Overridden from SearchBoxModelObserver:
  void SearchEngineChanged() override;
  void ShowAssistantChanged() override;

  // Updates the visibility of an IPH view.
  // If `can_show_iph` is false, delete the IPH view if it is visible.
  // If `can_show_iph` is true, show the IPH view when other conditions are met.
  void UpdateIphViewVisibility(bool can_show_iph);

  // Returns true if the event to trigger autocomplete should be handled.
  bool ShouldProcessAutocomplete();

  // Clear highlight range.
  void ResetHighlightRange();

  // Updates the kValue attribute of the search box textfield for accessibility.
  void UpdateAccessibleValue();

  // Updates the search box's text value.
  void SetText(const std::u16string& text);

  // Builds the menu model for the category filter menu. This returns a vector
  // of AppListSearchControlCategory that is shown in the filter menu.
  ui::SimpleMenuModel* BuildFilterMenuModel();

  // Returns the search categories that are available for users to choose if
  // they want to have the results in the categories displayed in launcher
  // search. These category will be listed in the filter menu for users to
  // toggle.
  std::vector<AppListSearchControlCategory> GetToggleableCategories();

  // Returns a map of enable states for each category, including the
  // non-toggleable ones. The result is used for metrics.
  CategoryEnableStateMap GetSearchCategoryEnableState();

  // Tracks whether the search result page view is visible.
  bool search_result_page_visible_ = false;

  // Tracks the current app list state.
  AppListState current_app_list_state_ = AppListState::kStateApps;

  std::u16string current_query_;

  QueryChangedCallback query_changed_callback_;

  // The range of highlighted text for autocomplete.
  gfx::Range highlight_range_;

  // The key most recently pressed.
  ui::KeyboardCode last_key_pressed_ = ui::VKEY_UNKNOWN;

  const raw_ptr<SearchBoxViewDelegate, DanglingUntriaged> delegate_;
  const raw_ptr<AppListViewDelegate> view_delegate_;

  // The layer that will draw the focus ring if needed. Could be a nullptr if
  // the search box is in the bubble launcher.
  std::unique_ptr<FocusRingLayer> focus_ring_layer_;

  // Whether the search box is embedded in the bubble launcher.
  const bool is_app_list_bubble_;

  // Whether the search box view should draw a highlight border.
  bool should_paint_highlight_border_ = false;

  // The corner radius of the search box background.
  int corner_radius_ = 0;

  // Whether an IPH is allowed to be shown or not.
  bool is_iph_allowed_ = false;

  // The category filter menu adapter and model that handles the menu life cycle
  // and command execution.
  std::unique_ptr<ui::SimpleMenuModel> filter_menu_model_;
  std::unique_ptr<FilterMenuAdapter> filter_menu_adapter_;

  // Set by SearchResultPageView when the accessibility selection moves to a
  // search result view - the value is the ID of the currently selected result
  // view.
  std::optional<ui::AXPlatformNodeId> a11y_active_descendant_;

  // Owned by SearchResultPageView (for fullscreen launcher) or
  // ProductivityLauncherSearchPage (for bubble launcher).
  raw_ptr<ResultSelectionController, DanglingUntriaged>
      result_selection_controller_ = nullptr;

  // The timestamp taken when the search box model's query is updated by the
  // user. Used in metrics. Metrics are only recorded for search model updates
  // that occur after a search has been initiated.
  base::TimeTicks user_initiated_model_update_time_;

  // If true, `SelectPlaceholderText()` always returns a fixed placeholder text
  // instead of the one picked randomly.
  bool use_fixed_placeholder_text_for_test_ = false;

  const bool is_jelly_enabled_ = false;

  base::ScopedObservation<SearchBoxModel, SearchBoxModelObserver>
      search_box_model_observer_{this};

  base::ScopedObservation<AssistantViewDelegate, AssistantViewDelegateObserver>
      assistant_view_delegate_observer_{this};

  base::WeakPtrFactory<SearchBoxView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
