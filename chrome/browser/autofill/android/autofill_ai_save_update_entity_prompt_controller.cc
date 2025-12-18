// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/containers/contains.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_view.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillAiSaveUpdateEntityPromptController_jni.h"

namespace autofill {

AutofillAiSaveUpdateEntityPromptController::
    AutofillAiSaveUpdateEntityPromptController(
        std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view,
        EntityTypeName entity_type_name,
        AutofillClient::EntityImportPromptResultCallback prompt_closed_callback)
    : prompt_view_(std::move(prompt_view)),
      entity_type_name_(entity_type_name),
      prompt_closed_callback_(std::move(prompt_closed_callback)),
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
  return GetPromptTitle(entity_type_name_, /*is_save_prompt=*/true);
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

base::android::ScopedJavaLocalRef<jobject>
AutofillAiSaveUpdateEntityPromptController::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void AutofillAiSaveUpdateEntityPromptController::OnUserAccepted(JNIEnv* env) {
  had_user_interaction_ = true;
  RunPromptClosedCallback(
      AutofillClient::AutofillAiBubbleClosedReason::kAccepted);
}

void AutofillAiSaveUpdateEntityPromptController::OnUserDeclined(JNIEnv* env) {
  had_user_interaction_ = true;
  RunPromptClosedCallback(
      AutofillClient::AutofillAiBubbleClosedReason::kCancelled);
}

void AutofillAiSaveUpdateEntityPromptController::OnPromptDismissed(
    JNIEnv* env) {
  RunPromptClosedCallback(
      AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted);
}

void AutofillAiSaveUpdateEntityPromptController::RunPromptClosedCallback(
    AutofillClient::AutofillAiBubbleClosedReason decision) {
  if (prompt_closed_callback_) {
    std::move(prompt_closed_callback_).Run(decision);
  }
}

}  // namespace autofill

DEFINE_JNI(AutofillAiSaveUpdateEntityPromptController)
