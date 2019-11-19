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

  MOCK_METHOD1(OnFillingTriggered, void(const autofill::UserInfo::Field&));
  MOCK_METHOD1(OnOptionSelected, void(autofill::AccessoryAction));
  MOCK_METHOD0(RefreshSuggestions, void());
  MOCK_METHOD0(OnPersonalDataChanged, void());
  MOCK_METHOD3(OnCreditCardFetched,
               void(bool, const autofill::CreditCard*, const base::string16&));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
