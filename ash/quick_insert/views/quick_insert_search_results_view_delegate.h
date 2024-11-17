// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_DELEGATE_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"

namespace views {
class View;
}

namespace ash {

enum class QuickInsertActionType;
enum class QuickInsertSectionType;

// Delegate for `QuickInsertSearchResultsView`.
class ASH_EXPORT QuickInsertSearchResultsViewDelegate {
 public:
  virtual void SelectSearchResult(const QuickInsertSearchResult& result) = 0;

  virtual void SelectMoreResults(QuickInsertSectionType type) = 0;

  // Requests for `view` to become the pseudo focused view.
  virtual void RequestPseudoFocus(views::View* view) = 0;

  virtual QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) = 0;

  // Informs that the height of the search results view may change.
  virtual void OnSearchResultsViewHeightChanged() = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_RESULTS_VIEW_DELEGATE_H_
