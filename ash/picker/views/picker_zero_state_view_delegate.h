// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace views {
class View;
}

namespace ash {

enum class PickerActionType;
enum class PickerCapsLockPosition;

// Delegate for `PickerZeroStateView`.
class ASH_EXPORT PickerZeroStateViewDelegate {
 public:
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult>)>;

  using SuggestedResultsCallback =
      base::RepeatingCallback<void(std::vector<PickerSearchResult>)>;

  virtual void SelectZeroStateCategory(PickerCategory category) = 0;

  virtual void SelectZeroStateResult(const PickerSearchResult& result) = 0;

  virtual void GetZeroStateSuggestedResults(
      SuggestedResultsCallback callback) = 0;

  // Requests for `view` to become the pseudo focused view.
  virtual void RequestPseudoFocus(views::View* view) = 0;

  virtual PickerActionType GetActionForResult(
      const PickerSearchResult& result) = 0;

  // Informs that the height of the zero state view may change.
  virtual void OnZeroStateViewHeightChanged() = 0;

  virtual PickerCapsLockPosition GetCapsLockPosition() = 0;

  virtual void SetCapsLockDisplayed(bool displayed) = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ZERO_STATE_VIEW_DELEGATE_H_
