// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {

enum class PickerActionType;
enum class PickerSectionType;
class PickerSearchResult;

// Delegate for `PickerSearchResultsView`.
class ASH_EXPORT PickerSearchResultsViewDelegate {
 public:
  virtual void SelectSearchResult(const PickerSearchResult& result) = 0;

  virtual void SelectMoreResults(PickerSectionType type) = 0;

  // `view` may be `nullptr` if there's no pseudo focused view.
  virtual void NotifyPseudoFocusChanged(views::View* view) = 0;

  virtual PickerActionType GetActionForResult(
      const PickerSearchResult& result) = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
