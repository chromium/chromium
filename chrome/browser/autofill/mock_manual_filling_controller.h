// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_

#include "chrome/browser/autofill/manual_filling_controller.h"
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

  MOCK_METHOD1(RefreshSuggestions, void(const autofill::AccessorySheetData&));
  MOCK_METHOD2(NotifyFocusedInputChanged,
               void(autofill::FieldRendererId,
                    autofill::mojom::FocusedFieldType));
  MOCK_METHOD2(UpdateSourceAvailability,
               void(ManualFillingController::FillingSource, bool));
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(OnAutomaticGenerationStatusChanged, void(bool));
  MOCK_METHOD1(ShowAccessorySheetTab, void(const autofill::AccessoryTabType&));
  MOCK_METHOD2(OnFillingTriggered,
               void(autofill::AccessoryTabType type,
                    const autofill::AccessorySheetField&));
  MOCK_CONST_METHOD1(OnOptionSelected,
                     void(autofill::AccessoryAction selected_action));
  MOCK_CONST_METHOD2(OnToggleChanged,
                     void(autofill::AccessoryAction toggled_action,
                          bool enabled));
  MOCK_METHOD2(RequestAccessorySheet,
               void(autofill::AccessoryTabType,
                    base::OnceCallback<void(autofill::AccessorySheetData)>));
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_MANUAL_FILLING_CONTROLLER_H_
