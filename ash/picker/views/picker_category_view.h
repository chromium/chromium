// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerSearchResult;
class PickerSectionView;

// View to show Picker results for a specific category.
class ASH_EXPORT PickerCategoryView : public views::View {
  METADATA_HEADER(PickerCategoryView, views::View)

 public:
  // Indicates the user has selected a result.
  using SelectResultCallback =
      base::OnceCallback<void(const PickerSearchResult& result)>;

  explicit PickerCategoryView(SelectResultCallback callback);
  PickerCategoryView(const PickerCategoryView&) = delete;
  PickerCategoryView& operator=(const PickerCategoryView&) = delete;
  ~PickerCategoryView() override;

  // Replaces the current results with `results`.
  void SetResults(const PickerSearchResults& results);

  base::span<const raw_ptr<PickerSectionView>> section_views_for_testing()
      const {
    return section_views_;
  }

 private:
  // Runs `select_result_callback_` on `result`. Note that only one result can
  // be selected (and subsequently calling this method will do nothing).
  void SelectResult(const PickerSearchResult& result);

  SelectResultCallback select_result_callback_;
  PickerSearchResults results_;

  // The views for each section of results.
  std::vector<raw_ptr<PickerSectionView>> section_views_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_CATEGORY_VIEW_H_
