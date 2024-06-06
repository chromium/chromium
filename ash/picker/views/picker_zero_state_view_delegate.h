// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace views {
class View;
}

namespace ash {

class PickerSearchResult;
enum class PickerActionType;

// Delegate for `PickerZeroStateView`.
class ASH_EXPORT PickerZeroStateViewDelegate {
 public:
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;

  using SearchResultsCallback =
      base::RepeatingCallback<void(std::vector<PickerSearchResult>)>;

  virtual void SelectZeroStateCategory(PickerCategory category) = 0;

  virtual void SelectZeroStateResult(const PickerSearchResult& result) = 0;

  virtual void GetZeroStateRecentResults(PickerCategory category,
                                         SearchResultsCallback callback) = 0;

  virtual void GetSuggestedZeroStateEditorResults(
      SuggestedEditorResultsCallback callback) = 0;

  // `view` may be `nullptr` if there's no pseudo focused view.
  virtual void NotifyPseudoFocusChanged(views::View* view) = 0;

  virtual PickerActionType GetActionForResult(
      const PickerSearchResult& result) = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_
