// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_

#include "chrome/browser/autofill/address_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAddressAccessoryController
    : public autofill::AddressAccessoryController {
 public:
  MockAddressAccessoryController();

  MockAddressAccessoryController(const MockAddressAccessoryController&) =
      delete;
  MockAddressAccessoryController& operator=(
      const MockAddressAccessoryController&) = delete;

  ~MockAddressAccessoryController() override;

  MOCK_METHOD(void,
              RegisterFillingSourceObserver,
              (FillingSourceObserver),
              (override));
  MOCK_METHOD(absl::optional<autofill::AccessorySheetData>,
              GetSheetData,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnFillingTriggered,
              (autofill::FieldGlobalId, const autofill::AccessorySheetField&),
              (override));
  MOCK_METHOD((void),
              OnPasskeySelected,
              (const std::vector<uint8_t>& credential_id),
              (override));
  MOCK_METHOD(void,
              OnOptionSelected,
              (autofill::AccessoryAction selected_action),
              (override));
  MOCK_METHOD(void,
              OnToggleChanged,
              (autofill::AccessoryAction toggled_action, bool enabled),
              (override));
  MOCK_METHOD(void, RefreshSuggestions, (), (override));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
