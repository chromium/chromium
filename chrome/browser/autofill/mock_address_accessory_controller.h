// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAddressAccessoryController
    : public autofill::AddressAccessoryController {
 public:
  MockAddressAccessoryController();
  ~MockAddressAccessoryController() override;

  MOCK_METHOD(void,
              RegisterFillingSourceObserver,
              (FillingSourceObserver),
              (override));
  MOCK_METHOD(base::Optional<autofill::AccessorySheetData>,
              GetSheetData,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnFillingTriggered,
              (autofill::FieldGlobalId, const autofill::UserInfo::Field&),
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAddressAccessoryController);
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
