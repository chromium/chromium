// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_

#include <optional>

#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/address_accessory_controller.h"
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
              RegisterPlusProfilesProvider,
              (base::WeakPtr<AffiliatedPlusProfilesProvider>),
              (override));
  MOCK_METHOD(void,
              RegisterFillingSourceObserver,
              (FillingSourceObserver),
              (override));
  MOCK_METHOD(std::optional<autofill::AccessorySheetData>,
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

  base::WeakPtr<AddressAccessoryController> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAddressAccessoryController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_ADDRESS_ACCESSORY_CONTROLLER_H_
