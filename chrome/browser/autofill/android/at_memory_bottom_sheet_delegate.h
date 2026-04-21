// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_

namespace autofill {

// Delegate interface for receiving events from the @memory bottom sheet.
class AtMemoryBottomSheetDelegate {
 public:
  virtual ~AtMemoryBottomSheetDelegate() = default;

  // Called when the bottom sheet is dismissed.
  virtual void OnDismissed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_AT_MEMORY_BOTTOM_SHEET_DELEGATE_H_
