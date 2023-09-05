// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_

#include "chrome/browser/autofill/manual_filling_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockManualFillingController
    : public ManualFillingController,
      public base::SupportsWeakPtr<MockManualFillingController> {
 public:
  MockManualFillingController();

  MockManualFillingController(const MockManualFillingController&) = delete;
  MockManualFillingController& operator=(const MockManualFillingController&) =
      delete;

  ~MockManualFillingController() override;

  MOCK_METHOD(void,
              RefreshSuggestions,
              (const autofill::AccessorySheetData&),
              (override));
  MOCK_METHOD((void),
              NotifyFocusedInputChanged,
              (autofill::FieldRendererId, autofill::mojom::FocusedFieldType),
              (override));
  MOCK_METHOD((void),
              UpdateSourceAvailability,
              (ManualFillingController::FillingSource, bool),
              (override));
  MOCK_METHOD((void), Hide, (), (override));
  MOCK_METHOD((void),
              OnAccessoryActionAvailabilityChanged,
              (ShouldShowAction, autofill::AccessoryAction),
              (override));
  MOCK_METHOD(void,
              ShowAccessorySheetTab,
              (const autofill::AccessoryTabType&),
              (override));
  MOCK_METHOD((void),
              OnFillingTriggered,
              (autofill::AccessoryTabType type,
               const autofill::AccessorySheetField&),
              (override));
  MOCK_METHOD((void),
              OnPasskeySelected,
              (autofill::AccessoryTabType type,
               const std::vector<uint8_t>& passkey_id),
              (override));
  MOCK_METHOD((void),
              OnOptionSelected,
              (autofill::AccessoryAction selected_action),
              (const, override));
  MOCK_METHOD((void),
              OnToggleChanged,
              (autofill::AccessoryAction toggled_action, bool enabled),
              (const, override));
  MOCK_METHOD((void),
              RequestAccessorySheet,
              (autofill::AccessoryTabType,
               base::OnceCallback<void(autofill::AccessorySheetData)>),
              (override));
  MOCK_METHOD((gfx::NativeView), container_view, (), (const, override));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
