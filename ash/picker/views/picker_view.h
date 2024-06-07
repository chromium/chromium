// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

enum class PickerLayoutType;
class PickerEmojiBarView;
class PickerMainContainerView;
class PickerSearchFieldView;
class PickerPageView;
class PickerSearchResult;
class PickerSearchResultsSection;
class PickerSearchResultsView;
class PickerViewDelegate;
class PickerZeroStateView;
class PickerCategoryView;

// View for the Picker widget.
class ASH_EXPORT PickerView : public views::WidgetDelegateView,
                              public PickerZeroStateViewDelegate,
                              public PickerSearchResultsViewDelegate,
                              public PickerPseudoFocusHandler {
  METADATA_HEADER(PickerView, views::WidgetDelegateView)

 public:
  // `delegate` must remain valid for the lifetime of this class.
  explicit PickerView(PickerViewDelegate* delegate,
                      PickerLayoutType layout_type,
                      base::TimeTicks trigger_event_timestamp);
  PickerView(const PickerView&) = delete;
  PickerView& operator=(const PickerView&) = delete;
  ~PickerView() override;

  // views::WidgetDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // PickerZeroStateViewDelegate:
  void SelectZeroStateCategory(PickerCategory category) override;
  void SelectZeroStateResult(const PickerSearchResult& result) override;
  void GetZeroStateRecentResults(PickerCategory category,
                                 SearchResultsCallback callback) override;
  void GetSuggestedZeroStateEditorResults(
      SuggestedEditorResultsCallback callback) override;
  void NotifyPseudoFocusChanged(views::View* view) override;

  // PickerSearchResultsViewDelegate:
  void SelectSearchResult(const PickerSearchResult& result) override;
  void SelectMoreResults(PickerSectionType type) override;
  PickerActionType GetActionForResult(
      const PickerSearchResult& result) override;

  // PickerPseudoFocusHandler:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  bool AdvancePseudoFocus(PseudoFocusDirection direction) override;
  bool GainPseudoFocus(PseudoFocusDirection direction) override;
  void LosePseudoFocus() override;

  // Returns the target bounds for this Picker view. The target bounds try to
  // vertically align `search_field_view_` with `anchor_bounds`. `anchor_bounds`
  // and returned bounds should be in screen coordinates.
  gfx::Rect GetTargetBounds(const gfx::Rect& anchor_bounds,
                            PickerLayoutType layout_type);

  PickerSearchFieldView& search_field_view_for_testing() {
    return *search_field_view_;
  }
  PickerSearchResultsView& search_results_view_for_testing() {
    return *search_results_view_;
  }
  PickerCategoryView& category_view_for_testing() { return *category_view_; }
  PickerZeroStateView& zero_state_view_for_testing() {
    return *zero_state_view_;
  }
  PickerEmojiBarView& emoji_bar_view_for_testing() { return *emoji_bar_view_; }

 private:
  // Starts a search with `query`, with search results being returned to
  // `PublishSearchResults`.
  void StartSearch(const std::u16string& query);

  // Displays `results` in the search view.
  // If `show_no_results_found` is true and `results` is empty, then a "no
  // results found" view is shown instead of a blank view.
  void PublishSearchResults(bool show_no_results_found,
                            std::vector<PickerSearchResultsSection> results);

  // Selects a category. This shows the category view and fetches zero-state
  // results for the category, which are returned to `PublishCategoryResults`.
  void SelectCategory(PickerCategory category);

  // Selects a category. This shows the category view and fetches search
  // results for the category based on `query`, which are returned to
  // `PublishSearchResults`.
  void SelectCategoryWithQuery(PickerCategory category,
                               std::u16string_view query);

  // Displays `results` in the category view.
  void PublishCategoryResults(std::vector<PickerSearchResultsSection> results);

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
  void AdvanceActivePseudoFocusHandler(PseudoFocusDirection direction);

  // Called when the search field back button is pressed.
  void OnSearchBackButtonPressed();

  // Clears the current results in the emoji bar and shows recent and
  // placeholder emojis instead.
  void ResetEmojiBarToZeroState();

  std::optional<PickerCategory> selected_category_;

  PickerKeyEventHandler key_event_handler_;
  PickerPerformanceMetrics performance_metrics_;
  raw_ptr<PickerViewDelegate> delegate_ = nullptr;

  // The main container contains the search field and contents pages.
  raw_ptr<PickerMainContainerView> main_container_view_ = nullptr;
  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<PickerZeroStateView> zero_state_view_ = nullptr;
  raw_ptr<PickerCategoryView> category_view_ = nullptr;
  raw_ptr<PickerSearchResultsView> search_results_view_ = nullptr;

  raw_ptr<PickerEmojiBarView> emoji_bar_view_ = nullptr;

  raw_ptr<PickerPseudoFocusHandler> active_pseudo_focus_handler_ = nullptr;

  // Whether the first set of results for the current search have been published
  // yet.
  bool published_first_results_ = false;

  base::WeakPtrFactory<PickerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
