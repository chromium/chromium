// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"

#include <utility>

#include "chrome/android/chrome_jni_headers/SaveAddressProfilePromptController_jni.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfilePromptController::SaveAddressProfilePromptController(
    std::unique_ptr<SaveAddressProfilePromptView> prompt_view,
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback decision_callback,
    base::OnceCallback<void()> dismissal_callback)
    : prompt_view_(std::move(prompt_view)),
      profile_(profile),
      decision_callback_(std::move(decision_callback)),
      dismissal_callback_(std::move(dismissal_callback)) {
  DCHECK(prompt_view_);
  DCHECK(decision_callback_);
  DCHECK(dismissal_callback_);
  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAddressProfileSavePrompt));
}

SaveAddressProfilePromptController::~SaveAddressProfilePromptController() {
  if (java_object_) {
    Java_SaveAddressProfilePromptController_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
  if (!had_user_interaction_) {
    RunSaveAddressProfileCallback(
        AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
        profile_);
  }
}

void SaveAddressProfilePromptController::DisplayPrompt() {
  bool success = prompt_view_->Show(this, profile_);
  if (!success)
    std::move(dismissal_callback_).Run();
}

std::u16string SaveAddressProfilePromptController::GetAddress() {
  return GetEnvelopeStyleAddress(profile_,
                                 g_browser_process->GetApplicationLocale(),
                                 /*include_country=*/true);
}

std::u16string SaveAddressProfilePromptController::GetEmail() {
  return profile_.GetInfo(EMAIL_ADDRESS,
                          g_browser_process->GetApplicationLocale());
}

std::u16string SaveAddressProfilePromptController::GetPhoneNumber() {
  return profile_.GetInfo(PHONE_HOME_WHOLE_NUMBER,
                          g_browser_process->GetApplicationLocale());
}

base::android::ScopedJavaLocalRef<jobject>
SaveAddressProfilePromptController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_SaveAddressProfilePromptController_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void SaveAddressProfilePromptController::OnUserAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted, profile_);
}

void SaveAddressProfilePromptController::OnUserDeclined(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined, profile_);
}

void SaveAddressProfilePromptController::OnUserEdited(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  had_user_interaction_ = true;
  AutofillProfile edited_profile;
  PersonalDataManagerAndroid::PopulateNativeProfileFromJava(jprofile, env,
                                                            &edited_profile);
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEdited,
      edited_profile);
}

void SaveAddressProfilePromptController::OnPromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::move(dismissal_callback_).Run();
}

void SaveAddressProfilePromptController::RunSaveAddressProfileCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    const AutofillProfile& profile) {
  std::move(decision_callback_).Run(decision, profile);
}

}  // namespace autofill
