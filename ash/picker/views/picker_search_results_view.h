// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}

namespace ash {

class PickerAssetFetcher;
class PickerSearchResult;
class PickerSectionListView;
class PickerSectionView;

class ASH_EXPORT PickerSearchResultsView : public PickerPageView {
  METADATA_HEADER(PickerSearchResultsView, PickerPageView)

 public:
  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::OnceCallback<void(const PickerSearchResult& result)>;
  // Indicates the user has selected "see more" on a section.
  using SelectMoreResultsCallback =
      base::RepeatingCallback<void(PickerSectionType type)>;

  // `asset_fetcher` must remain valid for the lifetime of this class.
  explicit PickerSearchResultsView(
      int picker_view_width,
      SelectSearchResultCallback select_search_result_callback,
      SelectMoreResultsCallback select_more_results_callback,
      PickerAssetFetcher* asset_fetcher);
  PickerSearchResultsView(const PickerSearchResultsView&) = delete;
  PickerSearchResultsView& operator=(const PickerSearchResultsView&) = delete;
  ~PickerSearchResultsView() override;

  // PickerPageView:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  void AdvancePseudoFocus(PseudoFocusDirection direction) override;

  // Clears the search results.
  void ClearSearchResults();

  // Append `section` to the current set of search results.
  // TODO: b/325840864 - Merge with existing sections if needed.
  void AppendSearchResults(PickerSearchResultsSection section);

  PickerSectionListView* section_list_view_for_testing() {
    return section_list_view_;
  }

  base::span<const raw_ptr<PickerSectionView>> section_views_for_testing()
      const {
    return section_views_;
  }

 private:
  // Runs `select_search_result_callback_` on `result`. Note that only one
  // result can be selected (and subsequently calling this method will do
  // nothing).
  void SelectSearchResult(const PickerSearchResult& result);

  // Adds a result item view to `section_view` based on what type `result` is.
  void AddResultToSection(const PickerSearchResult& result,
                          PickerSectionView* section_view);

  void SetPseudoFocusedView(views::View* view);

  void ScrollPseudoFocusedViewToVisible();

  void OnTrailingLinkClicked(PickerSectionType section_type,
                             const ui::Event& event);

  SelectSearchResultCallback select_search_result_callback_;
  SelectMoreResultsCallback select_more_results_callback_;

  // `asset_fetcher` outlives `this`.
  raw_ptr<PickerAssetFetcher> asset_fetcher_ = nullptr;

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // Used to track the views for each section of results.
  std::vector<raw_ptr<PickerSectionView>> section_views_;

  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;

  PickerPreviewBubbleController preview_bubble_controller_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
