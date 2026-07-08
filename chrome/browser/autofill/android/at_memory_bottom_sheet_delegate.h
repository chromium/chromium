// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_

#include <string>

namespace autofill {

// Delegate interface for receiving events from the @memory bottom sheet.
class AtMemoryBottomSheetDelegate {
 public:
  virtual ~AtMemoryBottomSheetDelegate() = default;

  // Called when the bottom sheet is dismissed.
  virtual void OnDismissed() = 0;

  // Called when the search query changes.
  virtual void OnQuerySubmitted(const std::u16string& query) = 0;

  // Called when the search query text changes.
  virtual void OnQueryTextChanged(const std::u16string& query) = 0;

  // Called when the user selects a suggestion in the bottom sheet.
  virtual void OnSuggestionSelected(int position) = 0;

  // Called when the child suggestions are shown to the user in the flyout
  // screen of the bottom sheet.
  virtual void OnChildSuggestionsShown(int parent_position) = 0;

  // Called when the user selects a child suggestion in the flyout screen of the
  // bottom sheet.
  virtual void OnChildSuggestionSelected(int parent_position,
                                         int child_position) = 0;

  // Returns true if a search is currently in progress.
  virtual bool IsSearching() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_
