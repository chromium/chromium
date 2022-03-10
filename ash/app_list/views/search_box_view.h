// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <stdint.h>

#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_box_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/search_box/search_box_view_base.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class Textfield;
class View;
}  // namespace views

namespace ash {

class AppListView;
class AppListViewDelegate;
class ContentsView;
class ResultSelectionController;
class SearchResultBaseView;

// Subclass of SearchBoxViewBase. SearchBoxModel is its data model
// that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class ASH_EXPORT SearchBoxView : public SearchBoxViewBase,
                                 public AppListModelProvider::Observer,
                                 public SearchBoxModelObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate,
                AppListView* app_list_view = nullptr);

  SearchBoxView(const SearchBoxView&) = delete;
  SearchBoxView& operator=(const SearchBoxView&) = delete;

  ~SearchBoxView() override;

  // Must be called before the user interacts with the search box. Cannot be
  // part of Init() because the controller isn't available until after Init()
  // is called.
  void SetResultSelectionController(ResultSelectionController* controller);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Resets state of SearchBoxView so it can be reshown.
  void ResetForShow();

  // Returns the total focus ring spacing for use in folders.
  static int GetFocusRingSpacing();

  // Overridden from SearchBoxViewBase:
  void Init(const InitParams& params) override;
  void UpdateSearchTextfieldAccessibleNodeData(
      ui::AXNodeData* node_data) override;
  void ClearSearch() override;
  void HandleSearchBoxEvent(ui::LocatedEvent* located_event) override;
  void UpdateKeyboardVisibility() override;
  void UpdateModel(bool initiated_by_user) override;
  void UpdateSearchIcon() override;
  void UpdatePlaceholderTextStyle() override;
  void UpdateSearchBoxBorder() override;
  void SetupAssistantButton() override;
  void SetupCloseButton() override;
  void SetupBackButton() override;
  void RecordSearchBoxActivationHistogram(ui::EventType event_type) override;
  void OnSearchBoxActiveChanged(bool active) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Overridden from views::View:
  void OnKeyEvent(ui::KeyEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // Updates the search box's background corner radius and color based on the
  // state of AppListModel.
  void UpdateBackground(AppListState target_state);

  // Updates the search box's layout based on the state of AppListModel.
  void UpdateLayout(AppListState target_state, int target_state_height);

  // Returns background border corner radius in the given state.
  int GetSearchBoxBorderCornerRadiusForState(AppListState state) const;

  // Returns background color for the given state.
  SkColor GetBackgroundColorForState(AppListState state) const;

  // Shows Zero State suggestions.
  void ShowZeroStateSuggestions();

  // Called when the wallpaper colors change.
  void OnWallpaperColorsChanged();

  // Sets the autocomplete text if autocomplete conditions are met.
  void ProcessAutocomplete(SearchResultBaseView* first_result_view);

  // Removes all autocomplete text.
  void ClearAutocompleteText();

  // Updates the search box with |new_query| and starts a new search.
  void UpdateQuery(const std::u16string& new_query);

  // Clears the search query and de-activate the search box.
  void ClearSearchAndDeactivateSearchBox();

  // Sets the view accessibility ID of the search box's active descendant.
  // The active descendant should be the currently selected result view in the
  // search results list.
  // `nullopt` indicates no active descendant, i.e. that no result is selected.
  void SetA11yActiveDescendant(
      const absl::optional<int32_t>& active_descendant);

  void set_contents_view(ContentsView* contents_view) {
    contents_view_ = contents_view;
  }
  ContentsView* contents_view() { return contents_view_; }

  ResultSelectionController* result_selection_controller_for_test() {
    return result_selection_controller_;
  }
  void set_highlight_range_for_test(const gfx::Range& range) {
    highlight_range_ = range;
  }

  // Update search box view background when result container visibility changes.
  void OnResultContainerVisibilityChanged(bool visible);

  // Whether the search box has a non-empty, non-whitespace query.
  bool HasValidQuery();

 private:
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

  // Overridden from views::TextfieldController:
  void OnBeforeUserAction(views::Textfield* sender) override;
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  // Overridden from SearchBoxModelObserver:
  void Update() override;
  void SearchEngineChanged() override;
  void ShowAssistantChanged() override;

  // Updates search_box() text to match |selected_result|. Should be called
  // when the selected search result changes.
  void UpdateSearchBoxTextForSelectedResult(SearchResult* selected_result);

  // Returns true if the event to trigger autocomplete should be handled.
  bool ShouldProcessAutocomplete();

  // Clear highlight range.
  void ResetHighlightRange();

  // Tracks whether the search result page view is visible.
  bool search_result_page_visible_ = false;

  // Tracks the current app list state.
  AppListState current_app_list_state_ = AppListState::kStateApps;

  std::u16string current_query_;

  // The range of highlighted text for autocomplete.
  gfx::Range highlight_range_;

  // The key most recently pressed.
  ui::KeyboardCode last_key_pressed_ = ui::VKEY_UNKNOWN;

  AppListViewDelegate* const view_delegate_;

  // Owned by views hierarchy. May be null for bubble launcher.
  AppListView* const app_list_view_;

  // Owned by views hierarchy. May be null for bubble launcher.
  ContentsView* contents_view_ = nullptr;

  // Whether the search box is embedded in the bubble launcher.
  const bool is_app_list_bubble_;

  // Whether tablet mode is active.
  bool is_tablet_mode_;

  // Set by SearchResultPageView when the accessibility selection moves to a
  // search result view - the value is the ID of the currently selected result
  // view.
  absl::optional<int32_t> a11y_active_descendant_;

  // Owned by SearchResultPageView (for fullscreen launcher) or
  // ProductivityLauncherSearchPage (for bubble launcher).
  ResultSelectionController* result_selection_controller_ = nullptr;

  // The timestamp taken when the search box model's query is updated by the
  // user. Used in metrics. Metrics are only recorded for search model updates
  // that occur after a search has been initiated.
  base::TimeTicks user_initiated_model_update_time_;

  base::ScopedObservation<SearchBoxModel, SearchBoxModelObserver>
      search_box_model_observer_{this};

  base::WeakPtrFactory<SearchBoxView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
