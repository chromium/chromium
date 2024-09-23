// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_

#include <optional>

#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPaymentMethodAccessoryController
    : public autofill::PaymentMethodAccessoryController {
 public:
  MockPaymentMethodAccessoryController();
  ~MockPaymentMethodAccessoryController() override;
  MockPaymentMethodAccessoryController(const MockPaymentMethodAccessoryController&) =
      delete;
  MockPaymentMethodAccessoryController& operator=(
      const MockPaymentMethodAccessoryController&) = delete;

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
  MOCK_METHOD(void, OnOptionSelected, (autofill::AccessoryAction), (override));
  MOCK_METHOD(void,
              OnToggleChanged,
              (autofill::AccessoryAction, bool),
              (override));
  MOCK_METHOD(void, RefreshSuggestions, (), (override));
  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));

  base::WeakPtr<PaymentMethodAccessoryController> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPaymentMethodAccessoryController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_PAYMENT_METHOD_ACCESSORY_CONTROLLER_H_
