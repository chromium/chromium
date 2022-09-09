// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>

#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordAccessoryController : public PasswordAccessoryController {
 public:
  MockPasswordAccessoryController();

  MockPasswordAccessoryController(const MockPasswordAccessoryController&) =
      delete;
  MockPasswordAccessoryController& operator=(
      const MockPasswordAccessoryController&) = delete;

  ~MockPasswordAccessoryController() override;

  MOCK_METHOD(
      void,
      SavePasswordsForOrigin,
      ((const std::map<std::u16string, const password_manager::PasswordForm*>&),
       (const url::Origin&)));
  MOCK_METHOD(void,
              RefreshSuggestionsForField,
              (autofill::mojom::FocusedFieldType, bool),
              (override));
  MOCK_METHOD(void,
              OnGenerationRequested,
              (autofill::password_generation::PasswordGenerationType),
              (override));
  MOCK_METHOD(void, DidNavigateMainFrame, ());
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
  MOCK_METHOD(void,
              OnOptionSelected,
              (autofill::AccessoryAction selected_action),
              (override));
  MOCK_METHOD(void,
              OnToggleChanged,
              (autofill::AccessoryAction toggled_action, bool enabled),
              (override));
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_
