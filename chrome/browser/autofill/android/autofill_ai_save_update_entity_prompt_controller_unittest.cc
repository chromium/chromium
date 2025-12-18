// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/android/mock_autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using ::testing::_;

class AutofillAiSaveUpdateEntityPromptControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillAiSaveUpdateEntityPromptControllerTest() {
    std::unique_ptr<MockAutofillAiSaveUpdateEntityPromptView> prompt_view =
        std::make_unique<MockAutofillAiSaveUpdateEntityPromptView>();
    prompt_view_ = prompt_view.get();

    controller_ = std::make_unique<AutofillAiSaveUpdateEntityPromptController>(
        std::move(prompt_view), EntityTypeName::kPassport,
        prompt_closed_callback_.Get());
  }

  MockAutofillAiSaveUpdateEntityPromptView& prompt_view() {
    return *prompt_view_.get();
  }

  AutofillAiSaveUpdateEntityPromptController& prompt_controller() {
    return *controller_.get();
  }

  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>&
  prompt_closed_callback() {
    return prompt_closed_callback_;
  }

  JNIEnv* env() { return env_.get(); }

 private:
  raw_ptr<MockAutofillAiSaveUpdateEntityPromptView> prompt_view_;
  std::unique_ptr<AutofillAiSaveUpdateEntityPromptController> controller_;
  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>
      prompt_closed_callback_;
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::JavaRef<jobject> mock_caller_{nullptr};
};

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_UserAccepted) {
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleClosedReason::kAccepted));
  // Both `OnUserAccepted` and `OnPromptDismissed` are called when the user
  // clicks the positive button.
  prompt_controller().OnUserAccepted(env());
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_UserDeclined) {
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleClosedReason::kCancelled));
  // Both `OnUserDeclined` and `OnPromptDismissed` are called when the user
  // clicks the negative button.
  prompt_controller().OnUserDeclined(env());
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_PromptDismissed) {
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(
      prompt_closed_callback(),
      Run(AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted));
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest, PromptUiStrings) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE),
            prompt_controller().GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON),
            prompt_controller().GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON),
      prompt_controller().GetNegativeButtonText());
}

}  // namespace
}  // namespace autofill
