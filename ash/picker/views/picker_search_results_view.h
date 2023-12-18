// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerSearchResult;

class PickerSearchResultsView : public views::View {
 public:
  METADATA_HEADER(PickerSearchResultsView);

  // Indicates the user has selected a result.
  using SelectSearchResultCallback =
      base::OnceCallback<void(const PickerSearchResult& result)>;

  explicit PickerSearchResultsView(SelectSearchResultCallback callback);
  PickerSearchResultsView(const PickerSearchResultsView&) = delete;
  PickerSearchResultsView& operator=(const PickerSearchResultsView&) = delete;
  ~PickerSearchResultsView() override;

 private:
  SelectSearchResultCallback select_search_result_callback_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
