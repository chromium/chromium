// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_DELEGATE_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace views {
class View;
}

namespace ash {

enum class QuickInsertActionType;
enum class PickerCapsLockPosition;

// Delegate for `PickerZeroStateView`.
class ASH_EXPORT PickerZeroStateViewDelegate {
 public:
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<QuickInsertSearchResult>)>;

  using SuggestedResultsCallback =
      base::RepeatingCallback<void(std::vector<QuickInsertSearchResult>)>;

  virtual void SelectZeroStateCategory(QuickInsertCategory category) = 0;

  virtual void SelectZeroStateResult(const QuickInsertSearchResult& result) = 0;

  virtual void GetZeroStateSuggestedResults(
      SuggestedResultsCallback callback) = 0;

  // Requests for `view` to become the pseudo focused view.
  virtual void RequestPseudoFocus(views::View* view) = 0;

  virtual QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) = 0;

  // Informs that the height of the zero state view may change.
  virtual void OnZeroStateViewHeightChanged() = 0;

  virtual PickerCapsLockPosition GetCapsLockPosition() = 0;

  virtual void SetCapsLockDisplayed(bool displayed) = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_DELEGATE_H_
