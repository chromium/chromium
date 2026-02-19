// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/browser/autofill/android/autofill_fallback_surface_launcher.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillAiSaveUpdateEntityPromptController_jni.h"

namespace autofill {

AutofillAiSaveUpdateEntityPromptController::
    AutofillAiSaveUpdateEntityPromptController(
        content::WebContents* web_contents,
        std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view,
        EntityInstance entity_instance,
        std::optional<EntityInstance> old_entity_instance,
        std::string app_locale,
        AutofillClient::EntityImportPromptResultCallback prompt_result_callback)
    : web_contents_(web_contents),
      prompt_view_(std::move(prompt_view)),
      entity_instance_(std::move(entity_instance)),
      old_entity_instance_(std::move(old_entity_instance)),
      app_locale_(std::move(app_locale)),
      prompt_result_callback_(std::move(prompt_result_callback)),
      java_object_(Java_AutofillAiSaveUpdateEntityPromptController_create(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))) {
  CHECK(prompt_view_);
}

AutofillAiSaveUpdateEntityPromptController::
    ~AutofillAiSaveUpdateEntityPromptController() {
  Java_AutofillAiSaveUpdateEntityPromptController_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_object_);
  java_object_.Reset();
}

void AutofillAiSaveUpdateEntityPromptController::DisplayPrompt() {
  prompt_view_->Show(this);
}

std::u16string AutofillAiSaveUpdateEntityPromptController::GetTitle() const {
  return GetPromptTitle(entity_instance_.type().name(),
                        /*is_save_prompt=*/!old_entity_instance_.has_value());
}

std::u16string
AutofillAiSaveUpdateEntityPromptController::GetPositiveButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON);
}

std::u16string
AutofillAiSaveUpdateEntityPromptController::GetNegativeButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON);
}

std::vector<EntityAttributeUpdateDetails>
AutofillAiSaveUpdateEntityPromptController::GetEntityUpdateDetails() const {
  return EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
      entity_instance_, old_entity_instance_, app_locale_);
}

std::u16string AutofillAiSaveUpdateEntityPromptController::GetSourceNotice()
    const {
  if (!IsWalletableEntity()) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_AI_SAVE_OR_UPDATE_LOCAL_ENTITY_SOURCE_NOTICE);
  }

  std::optional<AccountInfo> account = GetPrimaryAccountInfoFromBrowserContext(
      web_contents_->GetBrowserContext());
  if (!account) {
    return std::u16string();
  }

  const std::u16string google_wallet_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_AI_SAVE_OR_UPDATE_ENTITY_IN_WALLET_SOURCE_NOTICE,
      google_wallet_text, base::UTF8ToUTF16(account->email));
}

bool AutofillAiSaveUpdateEntityPromptController::IsWalletableEntity() const {
  return entity_instance_.record_type() ==
         EntityInstance::RecordType::kServerWallet;
}

bool AutofillAiSaveUpdateEntityPromptController::IsUpdatePrompt() const {
  return old_entity_instance_.has_value();
}

base::android::ScopedJavaLocalRef<jobject>
AutofillAiSaveUpdateEntityPromptController::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void AutofillAiSaveUpdateEntityPromptController::OpenManagePasses(JNIEnv* env) {
  ShowGoogleWalletPassesPage(*web_contents_);
}

void AutofillAiSaveUpdateEntityPromptController::OnUserAccepted(JNIEnv* env) {
  had_user_interaction_ = true;
  RunPromptClosedCallback(AutofillClient::AutofillAiBubbleResult::kAccepted);
}

void AutofillAiSaveUpdateEntityPromptController::OnUserDeclined(JNIEnv* env) {
  had_user_interaction_ = true;
  RunPromptClosedCallback(AutofillClient::AutofillAiBubbleResult::kCancelled);
}

void AutofillAiSaveUpdateEntityPromptController::OnPromptDismissed(
    JNIEnv* env) {
  RunPromptClosedCallback(
      AutofillClient::AutofillAiBubbleResult::kNotInteracted);
}

void AutofillAiSaveUpdateEntityPromptController::RunPromptClosedCallback(
    AutofillClient::AutofillAiBubbleResult result) {
  if (prompt_result_callback_) {
    std::move(prompt_result_callback_).Run(result);
  }
}

}  // namespace autofill

DEFINE_JNI(AutofillAiSaveUpdateEntityPromptController)
