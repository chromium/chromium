// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/android/mock_autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using ::testing::_;

class AutofillAiSaveUpdateEntityPromptControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            ChromeRenderViewHostTestHarness::profile());
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void CreateController(EntityInstance::RecordType record_type =
                            EntityInstance::RecordType::kLocal,
                        bool entity_updated = false) {
    std::unique_ptr<MockAutofillAiSaveUpdateEntityPromptView> prompt_view =
        std::make_unique<MockAutofillAiSaveUpdateEntityPromptView>();
    prompt_view_ = prompt_view.get();
    controller_ = std::make_unique<AutofillAiSaveUpdateEntityPromptController>(
        web_contents(), std::move(prompt_view),
        test::GetPassportEntityInstance(
            {.name = u"Jon doe", .record_type = record_type}),
        (entity_updated ? std::optional(test::GetPassportEntityInstance(
                              {.name = u"Seb doe", .record_type = record_type}))
                        : std::nullopt),
        "en-US", prompt_closed_callback_.Get());
  }

  void SigninUser(const std::string& email,
                  signin::ConsentLevel consent_level) {
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(email, consent_level);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
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
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<MockAutofillAiSaveUpdateEntityPromptView> prompt_view_;
  std::unique_ptr<AutofillAiSaveUpdateEntityPromptController> controller_;
  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>
      prompt_closed_callback_;
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::JavaRef<jobject> mock_caller_{nullptr};
};

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_UserAccepted) {
  CreateController();
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kAccepted));
  // Both `OnUserAccepted` and `OnPromptDismissed` are called when the user
  // clicks the positive button.
  prompt_controller().OnUserAccepted(env());
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_UserDeclined) {
  CreateController();
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kCancelled));
  // Both `OnUserDeclined` and `OnPromptDismissed` are called when the user
  // clicks the negative button.
  prompt_controller().OnUserDeclined(env());
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       DisplayPrompt_PromptDismissed) {
  CreateController();
  EXPECT_CALL(prompt_view(), Show(&prompt_controller()));
  prompt_controller().DisplayPrompt();

  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kNotInteracted));
  prompt_controller().OnPromptDismissed(env());
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       PromptUiStrings_SaveLocalEntity) {
  CreateController();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID),
            prompt_controller().GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON),
            prompt_controller().GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON),
      prompt_controller().GetNegativeButtonText());

  EXPECT_THAT(prompt_controller().GetSourceNotice(),
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_AI_SAVE_OR_UPDATE_LOCAL_ENTITY_SOURCE_NOTICE));
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       PromptUiStrings_UpdateLocalEntity) {
  CreateController(EntityInstance::RecordType::kLocal,
                   /*entity_updated=*/true);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID),
            prompt_controller().GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON),
            prompt_controller().GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON),
      prompt_controller().GetNegativeButtonText());

  EXPECT_THAT(prompt_controller().GetSourceNotice(),
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_AI_SAVE_OR_UPDATE_LOCAL_ENTITY_SOURCE_NOTICE));
}

TEST_F(AutofillAiSaveUpdateEntityPromptControllerTest,
       PromptUiStrings_WalletEntity) {
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  CreateController(EntityInstance::RecordType::kServerWallet);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID),
            prompt_controller().GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON),
            prompt_controller().GetPositiveButtonText());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON),
      prompt_controller().GetNegativeButtonText());

  const std::u16string google_wallet =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_AI_SAVE_OR_UPDATE_ENTITY_IN_WALLET_SOURCE_NOTICE,
                google_wallet,
                base::UTF8ToUTF16(TestingProfile::kDefaultProfileUserName)),
            prompt_controller().GetSourceNotice());
}

}  // namespace
}  // namespace autofill
