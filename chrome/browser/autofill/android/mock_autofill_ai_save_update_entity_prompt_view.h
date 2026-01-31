// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_MOCK_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_MOCK_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_H_

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class AutofillAiSaveUpdateEntityPromptController;

class MockAutofillAiSaveUpdateEntityPromptView
    : public AutofillAiSaveUpdateEntityPromptView {
 public:
  MockAutofillAiSaveUpdateEntityPromptView();
  ~MockAutofillAiSaveUpdateEntityPromptView() override;

  MOCK_METHOD(bool,
              Show,
              (const AutofillAiSaveUpdateEntityPromptController*),
              (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_MOCK_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_H_
