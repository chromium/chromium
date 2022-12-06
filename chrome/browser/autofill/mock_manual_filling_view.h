// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_

#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockManualFillingView : public ManualFillingViewInterface {
 public:
  MockManualFillingView();

  MockManualFillingView(const MockManualFillingView&) = delete;
  MockManualFillingView& operator=(const MockManualFillingView&) = delete;

  ~MockManualFillingView() override;

  MOCK_METHOD1(OnItemsAvailable, void(autofill::AccessorySheetData));
  MOCK_METHOD1(OnAutomaticGenerationStatusChanged, void(bool));
  MOCK_METHOD0(CloseAccessorySheet, void());
  MOCK_METHOD0(SwapSheetWithKeyboard, void());
  MOCK_METHOD1(Show, void(WaitForKeyboard));
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(ShowAccessorySheetTab, void(const autofill::AccessoryTabType&));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_VIEW_H_
