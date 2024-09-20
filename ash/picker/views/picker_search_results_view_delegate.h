// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"

namespace views {
class View;
}

namespace ash {

enum class PickerActionType;
enum class PickerSectionType;

// Delegate for `PickerSearchResultsView`.
class ASH_EXPORT PickerSearchResultsViewDelegate {
 public:
  virtual void SelectSearchResult(const PickerSearchResult& result) = 0;

  virtual void SelectMoreResults(PickerSectionType type) = 0;

  // Requests for `view` to become the pseudo focused view.
  virtual void RequestPseudoFocus(views::View* view) = 0;

  virtual PickerActionType GetActionForResult(
      const PickerSearchResult& result) = 0;

  // Informs that the height of the search results view may change.
  virtual void OnSearchResultsViewHeightChanged() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
