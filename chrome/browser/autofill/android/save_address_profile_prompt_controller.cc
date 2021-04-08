// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"

#include <utility>

#include "chrome/android/chrome_jni_headers/SaveAddressProfilePromptController_jni.h"
#include "components/autofill/core/browser/autofill_client.h"
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
        AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored);
  }
}

void SaveAddressProfilePromptController::DisplayPrompt() {
  bool success = prompt_view_->Show(this);
  if (!success)
    OnPromptDismissed();
}

void SaveAddressProfilePromptController::OnAccepted() {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

void SaveAddressProfilePromptController::OnDeclined() {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

void SaveAddressProfilePromptController::OnPromptDismissed() {
  std::move(dismissal_callback_).Run();
}

void SaveAddressProfilePromptController::RunSaveAddressProfileCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(decision_callback_).Run(decision, profile_);
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
  OnAccepted();
}

void SaveAddressProfilePromptController::OnUserDeclined(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OnDeclined();
}

void SaveAddressProfilePromptController::OnPromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OnPromptDismissed();
}

}  // namespace autofill
