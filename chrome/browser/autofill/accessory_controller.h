// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Interface for the portions of type-specific manual filling controllers (e.g.,
// password, credit card) which interact with the generic
// ManualFillingController.
class AccessoryController {
 public:
  using IsFillingSourceAvailable =
      base::StrongAlias<class IsFillingSourceAvailableTag, bool>;
  using FillingSourceObserver =
      base::RepeatingCallback<void(AccessoryController*,
                                   IsFillingSourceAvailable)>;

  virtual ~AccessoryController() = default;

  // Registers the observer that needs to be notified whenever the availability
  // or the content of a sheet changes.
  virtual void RegisterFillingSourceObserver(
      FillingSourceObserver observer) = 0;

  // Reurns a absl::nullopt if the accessory controller can't provide any data.
  // If the controller can provide data, it returns a non-empty sheet that *can*
  // be in a loading state while the data is being fetched.
  // Use |RegisterFillingSourceObserver()| to repeatedly be notified about
  // changes in the sheet data.
  virtual absl::optional<autofill::AccessorySheetData> GetSheetData() const = 0;

  // Triggered when a user selects an item for filling. This handler is
  // responsible for propagating it so that it ultimately ends up in the form
  // in the content area.
  virtual void OnFillingTriggered(
      autofill::FieldGlobalId focused_field_id,
      const autofill::AccessorySheetField& selection) = 0;

  // Triggered when a user selects an option.
  virtual void OnOptionSelected(autofill::AccessoryAction selected_action) = 0;

  // Triggered when a user changes a toggle.
  virtual void OnToggleChanged(autofill::AccessoryAction toggled_action,
                               bool enabled) = 0;
};

#endif  // CHROME_BROWSER_AUTOFILL_ACCESSORY_CONTROLLER_H_
