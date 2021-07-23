// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/search_box/search_box_view_base.h"

namespace views {
class Textfield;
class View;
}  // namespace views

namespace ash {

class AppListView;
class AppListViewDelegate;
class ContentsView;
class ResultSelectionController;
class SearchModel;
class SearchResultBaseView;

// Subclass of SearchBoxViewBase. SearchBoxModel is its data model
// that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class ASH_EXPORT SearchBoxView : public SearchBoxViewBase,
                                 public SearchBoxModelObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate,
                AppListView* app_list_view = nullptr);
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
  void ClearSearch() override;
  void HandleSearchBoxEvent(ui::LocatedEvent* located_event) override;
  void ModelChanged() override;
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

  // Overridden from views::View:
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // Updates the search box's background corner radius and color based on the
  // state of AppListModel.
  void UpdateBackground(double progress,
                        AppListState current_state,
                        AppListState target_state);

  // Updates the search box's layout based on the state of AppListModel.
  void UpdateLayout(double progress,
                    AppListState current_state,
                    int current_state_height,
                    AppListState target_state,
                    int target_state_height);

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

  // Updates the search box with |new_query| and starts a new search.
  void UpdateQuery(const std::u16string& new_query);

  // Clears the search query and de-activate the search box.
  void ClearSearchAndDeactivateSearchBox();

  void set_contents_view(ContentsView* contents_view) {
    contents_view_ = contents_view;
  }
  ContentsView* contents_view() { return contents_view_; }

  void set_a11y_selection_on_search_result(bool value) {
    a11y_selection_on_search_result_ = value;
  }

  ResultSelectionController* result_selection_controller_for_test() {
    return result_selection_controller_;
  }
  void set_highlight_range_for_test(const gfx::Range& range) {
    highlight_range_ = range;
  }

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

  // Removes all autocomplete text.
  void ClearAutocompleteText();

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

  std::u16string current_query_;

  // The range of highlighted text for autocomplete.
  gfx::Range highlight_range_;

  // The key most recently pressed.
  ui::KeyboardCode last_key_pressed_ = ui::VKEY_UNKNOWN;

  AppListViewDelegate* const view_delegate_;
  SearchModel* search_model_ = nullptr;  // Owned by the profile-keyed service.

  // Owned by views hierarchy. May be null for bubble launcher.
  AppListView* const app_list_view_;

  // Owned by views hierarchy. May be null for bubble launcher.
  ContentsView* contents_view_ = nullptr;

  // Whether the search box is embedded in the bubble launcher.
  const bool is_app_list_bubble_;

  // Whether tablet mode is active.
  bool is_tablet_mode_;

  // Set by SearchResultPageView when the accessibility selection moves to a
  // search result view.
  bool a11y_selection_on_search_result_ = false;

  // Owned by SearchResultPageView (for fullscreen launcher) or
  // AppListBubbleSearchPage (for bubble launcher).
  ResultSelectionController* result_selection_controller_ = nullptr;

  base::WeakPtrFactory<SearchBoxView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchBoxView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
