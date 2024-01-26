// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerAssetFetcher;
class PickerSearchResults;

// View to show Picker results for a specific category.
class ASH_EXPORT PickerCategoryView : public views::View {
  METADATA_HEADER(PickerCategoryView, views::View)

 public:
  explicit PickerCategoryView(
      PickerSearchResultsView::SelectSearchResultCallback
          select_search_result_callback,
      PickerAssetFetcher* asset_fetcher);
  PickerCategoryView(const PickerCategoryView&) = delete;
  PickerCategoryView& operator=(const PickerCategoryView&) = delete;
  ~PickerCategoryView() override;

  // Replaces the current results with `results`.
  void SetResults(const PickerSearchResults& results);

 private:
  // Default view for displaying category results.
  // TODO: b/316936620 - Replace this with specific category pages.
  raw_ptr<PickerSearchResultsView> search_results_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_
