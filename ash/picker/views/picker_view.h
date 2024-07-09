// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_controller.h"
#include "ash/picker/views/picker_emoji_bar_view_delegate.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

enum class PickerLayoutType;
enum class PickerPseudoFocusDirection;
class PickerEmojiBarView;
class PickerMainContainerView;
class PickerSearchFieldView;
class PickerPageView;
class PickerSearchResult;
class PickerSearchResultsSection;
class PickerSearchResultsView;
class PickerTraversableItemContainer;
class PickerViewDelegate;
class PickerZeroStateView;

// View for the Picker widget.
class ASH_EXPORT PickerView : public views::WidgetDelegateView,
                              public PickerZeroStateViewDelegate,
                              public PickerSearchResultsViewDelegate,
                              public PickerEmojiBarViewDelegate,
                              public PickerPseudoFocusHandler,
                              public views::ViewObserver {
  METADATA_HEADER(PickerView, views::WidgetDelegateView)

 public:
  // `delegate` must remain valid for the lifetime of this class.
  explicit PickerView(PickerViewDelegate* delegate,
                      PickerLayoutType layout_type,
                      base::TimeTicks trigger_event_timestamp);
  PickerView(const PickerView&) = delete;
  PickerView& operator=(const PickerView&) = delete;
  ~PickerView() override;

  // Time from when a search starts to when the previous set of results are
  // cleared.
  // Slightly longer than the real burn in period to ensure empty results do not
  // flash on the screen before showing burn-in results.
  static constexpr base::TimeDelta kClearResultsTimeout =
      PickerController::kBurnInPeriod + base::Milliseconds(50);

  // views::WidgetDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // PickerZeroStateViewDelegate:
  void SelectZeroStateCategory(PickerCategory category) override;
  void SelectZeroStateResult(const PickerSearchResult& result) override;
  void GetZeroStateSuggestedResults(SuggestedResultsCallback callback) override;
  void RequestPseudoFocus(views::View* view) override;

  // PickerSearchResultsViewDelegate:
  void SelectSearchResult(const PickerSearchResult& result) override;
  void SelectMoreResults(PickerSectionType type) override;
  PickerActionType GetActionForResult(
      const PickerSearchResult& result) override;

  // PickerEmojiBarViewDelegate:
  void ShowEmojiPicker(ui::EmojiPickerCategory category) override;

  // PickerPseudoFocusHandler:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  bool AdvancePseudoFocus(PickerPseudoFocusDirection direction) override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // Returns the target bounds for this Picker view. The target bounds try to
  // vertically align `search_field_view_` with `anchor_bounds`. `anchor_bounds`
  // and returned bounds should be in screen coordinates.
  gfx::Rect GetTargetBounds(const gfx::Rect& anchor_bounds,
                            PickerLayoutType layout_type);

  PickerSubmenuController& submenu_controller_for_testing() {
    return submenu_controller_;
  }

  PickerSearchFieldView& search_field_view_for_testing() {
    return *search_field_view_;
  }
  PickerSearchResultsView& search_results_view_for_testing() {
    return *search_results_view_;
  }
  PickerSearchResultsView& category_results_view_for_testing() {
    return *category_results_view_;
  }
  PickerZeroStateView& zero_state_view_for_testing() {
    return *zero_state_view_;
  }
  PickerEmojiBarView* emoji_bar_view_for_testing() { return emoji_bar_view_; }

 private:
  // Sets the search text field's query text to the query, focuses it, then
  // starts a search.
  void StartSearchWithNewQuery(std::u16string query);

  // Starts a search with the current query, with search results being returned
  // to `PublishSearchResults` and `PublishEmojiResults`.
  // If the query is empty, this calls `StopSearch` instead.
  void StartSearch();

  // Stops any previous searches, and sets the active page to the zero state /
  // category results view.
  void StopSearch();

  // Displays `results` in the emoji bar.
  void PublishEmojiResults(std::vector<PickerSearchResult> results);

  // Clears the search results.
  void OnClearResultsTimerFired();

  // Displays `results` in the search view.
  // If `results` is empty and no results were previously published, then a "no
  // results found" view is shown instead of a blank view.
  void PublishSearchResults(std::vector<PickerSearchResultsSection> results);

  // Selects a category. This shows the category view and fetches zero-state
  // results for the category, which are returned to `PublishCategoryResults`.
  void SelectCategory(PickerCategory category);

  // Selects a category. This shows the category view and fetches search
  // results for the category based on `query`, which are returned to
  // `PublishSearchResults`.
  void SelectCategoryWithQuery(PickerCategory category,
                               std::u16string_view query);

  // Displays `results` in the category view.
  void PublishCategoryResults(PickerCategory category,
                              std::vector<PickerSearchResultsSection> results);

  // Adds the main container, which includes the search field and contents
  // pages.
  void AddMainContainerView(PickerLayoutType layout_type);

  // Adds the emoji bar, which contains emoji and other expression results and
  // is shown above the main container.
  void AddEmojiBarView();

  // Sets `page_view` as the active page in `main_container_view_`.
  void SetActivePage(PickerPageView* page_view);

  // Moves pseudo focus between different parts of the PickerView, i.e. between
  // the emoji bar and the main container.
  void AdvanceActiveItemContainer(PickerPseudoFocusDirection direction);

  // Sets `view` as the pseudo focused view, i.e. the view which responds to
  // user actions that trigger `DoPseudoFocusedAction`. If `view` is null,
  // pseudo focus instead moves back to the search field.
  void SetPseudoFocusedView(views::View* view);

  // Called when the search field back button is pressed.
  void OnSearchBackButtonPressed();

  // Clears the current results in the emoji bar and shows recent and
  // placeholder emojis instead.
  void ResetEmojiBarToZeroState();

  // Returns true if `view` is contained in a submenu of this PickerView.
  bool IsContainedInSubmenu(views::View* view);

  std::optional<PickerCategory> selected_category_;

  PickerKeyEventHandler key_event_handler_;
  PickerSubmenuController submenu_controller_;
  PickerPerformanceMetrics performance_metrics_;
  raw_ptr<PickerViewDelegate> delegate_ = nullptr;

  // The main container contains the search field and contents pages.
  raw_ptr<PickerMainContainerView> main_container_view_ = nullptr;
  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<PickerZeroStateView> zero_state_view_ = nullptr;
  raw_ptr<PickerSearchResultsView> category_results_view_ = nullptr;
  raw_ptr<PickerSearchResultsView> search_results_view_ = nullptr;

  raw_ptr<PickerEmojiBarView> emoji_bar_view_ = nullptr;

  // The item container which contains `pseudo_focused_view_` and will respond
  // to keyboard navigation events.
  raw_ptr<PickerTraversableItemContainer> active_item_container_ = nullptr;

  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;

  // Clears `search_results_view_`'s old search results when a new search is
  // started - after `kClearResultsTimeout`, or when the first search results
  // come in (whatever is earliest).
  // This timer is running iff the first set of results for the current search
  // have not been published yet.
  base::OneShotTimer clear_results_timer_;

  base::ScopedObservation<views::View, views::ViewObserver>
      pseudo_focused_view_observation_{this};

  base::WeakPtrFactory<PickerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
