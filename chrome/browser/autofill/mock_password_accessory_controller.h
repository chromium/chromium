// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>

#include "base/macros.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordAccessoryController : public PasswordAccessoryController {
 public:
  MockPasswordAccessoryController();
  ~MockPasswordAccessoryController() override;

  MOCK_METHOD2(SavePasswordsForOrigin,
               void(const std::map<base::string16,
                                   const password_manager::PasswordForm*>&,
                    const url::Origin&));
  MOCK_METHOD2(RefreshSuggestionsForField,
               void(autofill::mojom::FocusedFieldType, bool));
  MOCK_METHOD1(OnGenerationRequested,
               void(autofill::password_generation::PasswordGenerationType));
  MOCK_METHOD0(DidNavigateMainFrame, void());
  MOCK_METHOD1(OnFillingTriggered, void(const autofill::UserInfo::Field&));
  MOCK_METHOD1(OnOptionSelected,
               void(autofill::AccessoryAction selected_action));
  MOCK_METHOD2(OnToggleChanged,
               void(autofill::AccessoryAction toggled_action, bool enabled));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordAccessoryController);
};

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_PASSWORD_ACCESSORY_CONTROLLER_H_
