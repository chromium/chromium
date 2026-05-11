// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AT_MEMORY_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AT_MEMORY_ACCESSORY_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/at_memory_accessory_controller.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAtMemoryAccessoryController : public AtMemoryAccessoryController {
 public:
  MockAtMemoryAccessoryController();
  MockAtMemoryAccessoryController(const MockAtMemoryAccessoryController&) =
      delete;
  MockAtMemoryAccessoryController& operator=(
      const MockAtMemoryAccessoryController&) = delete;
  ~MockAtMemoryAccessoryController() override;

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
  MOCK_METHOD(void,
              OnPasskeySelected,
              (const std::vector<uint8_t>& passkey_id),
              (override));
  MOCK_METHOD(void,
              OnOptionSelected,
              (autofill::AccessoryAction selected_action),
              (override));
  MOCK_METHOD(void,
              OnToggleChanged,
              (autofill::AccessoryAction toggled_action, bool enabled),
              (override));

  base::WeakPtr<AtMemoryAccessoryController> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAtMemoryAccessoryController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AT_MEMORY_ACCESSORY_CONTROLLER_H_
