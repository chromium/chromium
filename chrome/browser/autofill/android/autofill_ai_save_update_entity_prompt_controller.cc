// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/browser/autofill/android/autofill_fallback_surface_launcher.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_util.h"
#include "components/autofill/core/browser/ui/autofill_ai/entity_attribute_update_details.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
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
        LegalMessageLines public_passes_notice,
        const base::i18n::LanguageTag& app_locale,
        AutofillClient::EntityImportPromptResultCallback prompt_result_callback)
    : web_contents_(web_contents),
      prompt_view_(std::move(prompt_view)),
      entity_instance_(std::move(entity_instance)),
      old_entity_instance_(std::move(old_entity_instance)),
      public_passes_notice_(std::move(public_passes_notice)),
      app_locale_(app_locale),
      prompt_result_callback_(std::move(prompt_result_callback)),
      java_object_(Java_AutofillAiSaveUpdateEntityPromptController_create(
          base::android::AttachCurrentThread(),
          jni_zero::MakeRaw(this))) {
  CHECK(prompt_view_);
  InitSourceNoticeAndUiContext();
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
                        /*is_save_prompt=*/!old_entity_instance_.has_value(),
                        /*is_banner_prompt=*/false,
                        /*is_server_wallet=*/IsWalletableEntity());
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

const std::u16string&
AutofillAiSaveUpdateEntityPromptController::GetSourceNotice() const {
  return source_notice_;
}

bool AutofillAiSaveUpdateEntityPromptController::IsWalletableEntity() const {
  return entity_instance_.record_type() ==
         EntityInstance::RecordType::kServerWallet;
}

bool AutofillAiSaveUpdateEntityPromptController::IsUpdatePrompt() const {
  return old_entity_instance_.has_value();
}

const LegalMessageLines&
AutofillAiSaveUpdateEntityPromptController::GetPublicPassesNotice() const {
  return public_passes_notice_;
}

bool AutofillAiSaveUpdateEntityPromptController::
    IsEligibleForWalletPassDisclosure() const {
  return autofill::IsEligibleForWalletPassDisclosure(
      /*is_save_prompt=*/!IsUpdatePrompt(), entity_instance_);
}

base::android::ScopedJavaLocalRef<jobject>
AutofillAiSaveUpdateEntityPromptController::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void AutofillAiSaveUpdateEntityPromptController::OnWalletLinkClicked(
    JNIEnv* env) {
  switch (GetWalletPassType(entity_instance_.type(),
                            entity_instance_.record_type())) {
    case EntityInstance::WalletPassType::kUnsupported:
      NOTREACHED();
    case EntityInstance::WalletPassType::kPrivate:
      ShowGoogleWallePrivatePassesHelpCenterPageInCct(*web_contents_);
      break;
    case EntityInstance::WalletPassType::kPublic:
      ShowGoogleWalletPassesPage(*web_contents_);
      break;
  }
}

void AutofillAiSaveUpdateEntityPromptController::OnPrivacyPolicyLinkClicked(
    JNIEnv* env) {
  ShowGooglePrivacyPolicyPageInCct(*web_contents_);
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

void AutofillAiSaveUpdateEntityPromptController::
    InitSourceNoticeAndUiContext() {
  if (IsWalletableEntity()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents_->GetBrowserContext());
    AndroidWalletSourceNotice notice = GetAndroidWalletSourceNotice(
        entity_instance_.type(), /*is_save_prompt=*/!IsUpdatePrompt(),
        account ? base::UTF8ToUTF16(account->GetEmail()) : std::u16string());
    source_notice_ = std::move(notice.text);
    ui_context_.accepted_consent_string_ids = std::move(notice.string_ids);
  } else {
    source_notice_ = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_AI_SAVE_OR_UPDATE_LOCAL_ENTITY_SOURCE_NOTICE);
    ui_context_.accepted_consent_string_ids = {
        IDS_AUTOFILL_AI_SAVE_OR_UPDATE_LOCAL_ENTITY_SOURCE_NOTICE};
  }
  ui_context_.accept_button_string_id =
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON;
}

void AutofillAiSaveUpdateEntityPromptController::RunPromptClosedCallback(
    AutofillClient::AutofillAiBubbleResult result) {
  if (prompt_result_callback_) {
    std::move(prompt_result_callback_)
        .Run(result, /*edited_entity=*/std::nullopt,
             DidUserExplicitlyAcceptedImportPrompt(result)
                 ? ui_context_
                 : AutofillClient::EntityImportUIContext{});
  }
}

}  // namespace autofill

DEFINE_JNI(AutofillAiSaveUpdateEntityPromptController)
