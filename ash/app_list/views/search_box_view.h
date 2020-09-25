// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_

#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model_observer.h"
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
class SearchModel;

// Subclass of SearchBoxViewBase. SearchBoxModel is its data model
// that controls what icon to display, what placeholder text to use for
// Textfield. The text and selection model part could be set to change the
// contents and selection model of the Textfield.
class APP_LIST_EXPORT SearchBoxView : public SearchBoxViewBase,
                                      public SearchBoxModelObserver {
 public:
  SearchBoxView(SearchBoxViewDelegate* delegate,
                AppListViewDelegate* view_delegate,
                AppListView* app_list_view = nullptr);
  ~SearchBoxView() override;

  void Init(bool is_tablet_mode);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Resets state of SearchBoxView so it can be reshown.
  void ResetForShow();

  // Returns the total focus ring spacing for use in folders.
  static int GetFocusRingSpacing();

  // Overridden from SearchBoxViewBase:
  void ClearSearch() override;
  views::View* GetSelectedViewInContentsView() override;
  void HandleSearchBoxEvent(ui::LocatedEvent* located_event) override;
  void ModelChanged() override;
  void UpdateKeyboardVisibility() override;
  void UpdateModel(bool initiated_by_user) override;
  void UpdateSearchIcon() override;
  void UpdateSearchBoxBorder() override;
  void SetupAssistantButton() override;
  void SetupCloseButton() override;
  void SetupBackButton() override;
  void RecordSearchBoxActivationHistogram(ui::EventType event_type) override;

  // Overridden from views::View:
  void OnKeyEvent(ui::KeyEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

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
  void ProcessAutocomplete();

  // Updates the search box with |new_query| and starts a new search.
  void UpdateQuery(const base::string16& new_query);

  // Clears the search query and de-activate the search box.
  void ClearSearchAndDeactivateSearchBox();

  void set_contents_view(ContentsView* contents_view) {
    contents_view_ = contents_view;
  }
  ContentsView* contents_view() { return contents_view_; }

  void set_highlight_range_for_test(const gfx::Range& range) {
    highlight_range_ = range;
  }

 private:
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
  void SetAutocompleteText(const base::string16& autocomplete_text);

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
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

  base::string16 current_query_;

  // The range of highlighted text for autocomplete.
  gfx::Range highlight_range_;

  // The key most recently pressed.
  ui::KeyboardCode last_key_pressed_ = ui::VKEY_UNKNOWN;

  AppListViewDelegate* view_delegate_;   // Not owned.
  SearchModel* search_model_ = nullptr;  // Owned by the profile-keyed service.

  // Owned by views hierarchy.
  AppListView* app_list_view_;
  ContentsView* contents_view_ = nullptr;

  // True if app list search autocomplete is enabled.
  const bool is_app_list_search_autocomplete_enabled_;


  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;

  base::WeakPtrFactory<SearchBoxView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchBoxView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_BOX_VIEW_H_
