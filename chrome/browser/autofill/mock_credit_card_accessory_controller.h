// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_CREDIT_CARD_ACCESSORY_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/autofill/credit_card_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockCreditCardAccessoryController
    : public autofill::CreditCardAccessoryController {
 public:
  MockCreditCardAccessoryController();
  ~MockCreditCardAccessoryController() override;
  MockCreditCardAccessoryController(const MockCreditCardAccessoryController&) =
      delete;
  MockCreditCardAccessoryController& operator=(
      const MockCreditCardAccessoryController&) = delete;

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
  MOCK_METHOD(void, OnOptionSelected, (autofill::AccessoryAction), (override));
  MOCK_METHOD(void,
              OnToggleChanged,
              (autofill::AccessoryAction, bool),
              (override));
  MOCK_METHOD(void, RefreshSuggestions, (), (override));
  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));
  MOCK_METHOD(void,
              OnCreditCardFetched,
              (bool, const autofill::CreditCard*, const std::u16string&),
              (override));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
