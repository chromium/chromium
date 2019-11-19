// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "url/gurl.h"

class ManualFillingController;

namespace autofill {
class AccessorySheetData;
}  // namespace autofill

// The interface for creating and controlling a view for the password accessory.
// The view gets data from a given |ManualFillingController| and forwards
// any request (like filling a suggestion) back to the controller.
class ManualFillingViewInterface {
 public:
  // Defines which item types exist.
  // TODO(crbug.com/902425): Remove this once AccessorySheetData is used on the
  //                         frontend to represent data to present.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.chrome.browser.autofill.keyboard_accessory)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ItemType
  enum class Type {
    // An item in title style to purely to display text. Non-interactive.
    LABEL = 1,  // e.g. the "Passwords for this site" section header.

    // An item in list style to displaying an interactive suggestion.
    SUGGESTION = 2,  // e.g. a user's email address used for sign-up.

    // An item in list style to displaying a non-interactive suggestion.
    NON_INTERACTIVE_SUGGESTION = 3,  // e.g. the "(No username)" suggestion.

    // A horizontal, non-interactive divider used to visually divide sections.
    DIVIDER = 4,

    // A single, usually static and interactive suggestion.
    OPTION = 5,  // e.g. the "Manage passwords..." link.

    // A horizontal, non-interactive divider used to visually divide the
    // accessory sheet from the accessory bar.
    TOP_DIVIDER = 6,
  };

  virtual ~ManualFillingViewInterface() = default;

  // Called with data that should replace the data currently shown in an
  // accessory sheet of the same type.
  virtual void OnItemsAvailable(const autofill::AccessorySheetData& data) = 0;

  // Called when the generation action should be offered or rescinded
  // in the keyboard accessory.
  virtual void OnAutomaticGenerationStatusChanged(bool available) = 0;

  // Called to inform the view that the accessory sheet should be closed now.
  virtual void CloseAccessorySheet() = 0;

  // Opens a keyboard which dismisses the sheet. NoOp without open sheet.
  virtual void SwapSheetWithKeyboard() = 0;

  // Shows the accessory bar when the keyboard is also shown.
  virtual void ShowWhenKeyboardIsVisible() = 0;

  // Hides the accessory bar and the accessory sheet (if open).
  virtual void Hide() = 0;

 private:
  friend class ManualFillingControllerImpl;
  // Factory function used to create a concrete instance of this view.
  static std::unique_ptr<ManualFillingViewInterface> Create(
      ManualFillingController* controller);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_VIEW_INTERFACE_H_
