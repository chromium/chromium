// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_IMPL_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/at_memory_accessory_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AccessorySheetData;
class AccessorySheetField;
enum class AccessoryAction;
}  // namespace autofill

class AtMemoryAccessoryControllerImpl
    : public AtMemoryAccessoryController,
      public content::WebContentsUserData<AtMemoryAccessoryControllerImpl> {
 public:
  AtMemoryAccessoryControllerImpl(const AtMemoryAccessoryControllerImpl&) =
      delete;
  AtMemoryAccessoryControllerImpl& operator=(
      const AtMemoryAccessoryControllerImpl&) = delete;
  ~AtMemoryAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  std::optional<autofill::AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(
      autofill::FieldGlobalId focused_field_id,
      const autofill::AccessorySheetField& selection) override;
  void OnPasskeySelected(const std::vector<uint8_t>& passkey_id) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;
  void OnToggleChanged(autofill::AccessoryAction toggled_action,
                       bool enabled) override;

  // AtMemoryAccessoryController:
  base::WeakPtr<AtMemoryAccessoryController> AsWeakPtr() override;

 private:
  friend class content::WebContentsUserData<AtMemoryAccessoryControllerImpl>;

  explicit AtMemoryAccessoryControllerImpl(content::WebContents* web_contents);

  base::WeakPtrFactory<AtMemoryAccessoryControllerImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_IMPL_H_
