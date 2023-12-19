// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerSearchResult;

class ASH_EXPORT PickerSearchResultsView : public views::View {
 public:
  METADATA_HEADER(PickerSearchResultsView);

  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::OnceCallback<void(const PickerSearchResult& result)>;

  explicit PickerSearchResultsView(SelectSearchResultCallback callback);
  PickerSearchResultsView(const PickerSearchResultsView&) = delete;
  PickerSearchResultsView& operator=(const PickerSearchResultsView&) = delete;
  ~PickerSearchResultsView() override;

  // Replaces the current search results with `results`.
  void SetSearchResults(const PickerSearchResults& results);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

 private:
  SelectSearchResultCallback select_search_result_callback_;
  PickerSearchResults search_results_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
