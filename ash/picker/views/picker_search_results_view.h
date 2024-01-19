// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerAssetFetcher;
class PickerItemView;
class PickerSearchResult;
class PickerSectionView;

class ASH_EXPORT PickerSearchResultsView : public views::View {
 public:
  METADATA_HEADER(PickerSearchResultsView);

  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::OnceCallback<void(const PickerSearchResult& result)>;

  // `asset_fetcher` must remain valid for the lifetime of this class.
  explicit PickerSearchResultsView(
      SelectSearchResultCallback select_search_result_callback,
      PickerAssetFetcher* asset_fetcher);
  PickerSearchResultsView(const PickerSearchResultsView&) = delete;
  PickerSearchResultsView& operator=(const PickerSearchResultsView&) = delete;
  ~PickerSearchResultsView() override;

  // Replaces the current search results with `results`.
  void SetSearchResults(const PickerSearchResults& results);

  base::span<const raw_ptr<PickerSectionView>> section_views_for_testing()
      const {
    return section_views_;
  }

 private:
  // Runs `select_search_result_callback_` on `result`. Note that only one
  // result can be selected (and subsequently calling this method will do
  // nothing).
  void SelectSearchResult(const PickerSearchResult& result);

  // Creates a result item view based on what type `result` is.
  std::unique_ptr<PickerItemView> CreateItemView(
      const PickerSearchResult& result);

  SelectSearchResultCallback select_search_result_callback_;
  PickerSearchResults search_results_;

  // `asset_fetcher` outlives `this`.
  raw_ptr<PickerAssetFetcher> asset_fetcher_ = nullptr;

  // The views for each section of results.
  std::vector<raw_ptr<PickerSectionView>> section_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
