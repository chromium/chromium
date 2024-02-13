// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_

#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockManualFillingView : public ManualFillingViewInterface {
 public:
  MockManualFillingView();

  MockManualFillingView(const MockManualFillingView&) = delete;
  MockManualFillingView& operator=(const MockManualFillingView&) = delete;

  ~MockManualFillingView() override;

  MOCK_METHOD((void),
              OnItemsAvailable,
              (autofill::AccessorySheetData),
              (override));
  MOCK_METHOD((void),
              OnAccessoryActionAvailabilityChanged,
              (ShouldShowAction, autofill::AccessoryAction),
              (override));
  MOCK_METHOD((void), CloseAccessorySheet, (), (override));
  MOCK_METHOD((void), SwapSheetWithKeyboard, (), (override));
  MOCK_METHOD((void), Show, (WaitForKeyboard), (override));
  MOCK_METHOD((void), Hide, (), (override));
  MOCK_METHOD((void),
              ShowAccessorySheetTab,
              (const autofill::AccessoryTabType&),
              (override));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_
