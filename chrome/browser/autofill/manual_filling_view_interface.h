// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_

#include <memory>
#include <vector>

#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"

class ManualFillingController;

namespace autofill {
class AccessorySheetData;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

// The interface for creating and controlling a view for the password accessory.
// The view gets data from a given `ManualFillingController` and forwards
// any request (like filling a suggestion) back to the controller.
class ManualFillingViewInterface {
 public:
  using WaitForKeyboard = base::StrongAlias<struct WaitForKeyboardTag, bool>;
  using ShouldShowOnLargeFormFactor =
      base::StrongAlias<struct ShouldShowOnLargeFormFactorTag, bool>;
  using IsContentEditable =
      base::StrongAlias<struct IsContentEditableTag, bool>;
  using ShouldShowAction = base::StrongAlias<struct ShouldShowActionTag, bool>;

  virtual ~ManualFillingViewInterface() = default;

  // Called with data that should replace the data currently shown in an
  // accessory sheet of the same type.
  virtual void OnItemsAvailable(autofill::AccessorySheetData data) = 0;

  // Called when a keyboard accessory action should be offered or rescinded.
  virtual void OnAccessoryActionAvailabilityChanged(
      ShouldShowAction shouldShowAction,
      autofill::AccessoryAction action) = 0;

  // Called to inform the view that the accessory sheet should be closed now.
  virtual void CloseAccessorySheet() = 0;

  // Opens a keyboard which dismisses the sheet. NoOp without open sheet.
  virtual void SwapSheetWithKeyboard() = 0;

  // Shows the accessory bar. If `wait_for_keyboard`, shows the bar when the
  // keyboard is also shown. `should_show_on_large_form_factor` controls whether
  // the accessory should be displayed on Large Form Factors (where it is
  // otherwise suppressed on empty fields). `is_content_editable` controls
  // whether the focused field is contenteditable (where only the AtMemory icon
  // is shown).
  virtual void Show(
      WaitForKeyboard wait_for_keyboard,
      ShouldShowOnLargeFormFactor should_show_on_large_form_factor,
      IsContentEditable is_content_editable) = 0;

  // Hides the accessory bar and the accessory sheet (if open).
  virtual void Hide() = 0;

  // Shows the accessory sheet for the given `tab_type`.
  virtual void ShowAccessorySheetTab(
      const autofill::AccessoryTabType& tab_type) = 0;

  // Returns true if the device is a large form factor.
  virtual bool IsLargeFormFactor() const = 0;

 private:
  friend class ManualFillingControllerImpl;
  // Factory function used to create a concrete instance of this view.
  static std::unique_ptr<ManualFillingViewInterface> Create(
      ManualFillingController* controller,
      content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_
