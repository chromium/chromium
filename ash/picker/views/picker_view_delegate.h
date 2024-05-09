// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace ash {

class PickerAssetFetcher;
class PickerSearchResult;
class PickerSearchResultsSection;
class PickerSessionMetrics;

// Delegate for `PickerView`.
class ASH_EXPORT PickerViewDelegate {
 public:
  using SearchResultsCallback = base::RepeatingCallback<void(
      std::vector<PickerSearchResultsSection> results)>;
  using SuggestedEditorResultsCallback =
      base::OnceCallback<void(std::vector<PickerSearchResult> results)>;

  virtual ~PickerViewDelegate() {}

  virtual std::vector<PickerCategory> GetAvailableCategories() = 0;

  // Returns whether we should show suggested results in zero state view.
  virtual bool ShouldShowSuggestedResults() = 0;

  // Gets initially suggested results for category. Results will be returned via
  // `callback`, which may be called multiples times to update the results.
  virtual void GetResultsForCategory(PickerCategory category,
                                     SearchResultsCallback callback) = 0;

  // Transforms the selected text specified by `category` then commits to the
  // focused input field. `category` should be one of   kUpperCase, kLowerCase,
  // kSentenceCase, kTitleCase.
  virtual void TransformSelectedText(PickerCategory category) = 0;

  // Starts a search for `query`. Results will be returned via `callback`,
  // which may be called multiples times to update the results.
  virtual void StartSearch(const std::u16string& query,
                           std::optional<PickerCategory> category,
                           SearchResultsCallback callback) = 0;

  // Inserts `result` into the next focused input field.
  // If there's no focus event within some timeout after the widget is closed,
  // the result is dropped silently.
  virtual void InsertResultOnNextFocus(const PickerSearchResult& result) = 0;

  // Opens `result`. The exact behavior varies on the type of result.
  virtual void OpenResult(const PickerSearchResult& result) = 0;

  // Shows the Emoji Picker with `category`.
  virtual void ShowEmojiPicker(ui::EmojiPickerCategory category,
                               std::u16string_view query) = 0;

  // Shows the Editor.
  virtual void ShowEditor(std::optional<std::string> preset_query_id,
                          std::optional<std::string> freeform_text) = 0;

  // Sets the current caps lock state.
  virtual void SetCapsLockEnabled(bool enabled) = 0;

  virtual void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) = 0;

  virtual PickerAssetFetcher* GetAssetFetcher() = 0;

  virtual PickerSessionMetrics& GetSessionMetrics() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
