// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"

#include <utility>

#include "chrome/android/chrome_jni_headers/SaveAddressProfilePromptController_jni.h"
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

std::u16string SaveAddressProfilePromptController::GetAddress() {
  const std::string locale = g_browser_process->GetApplicationLocale();
  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const std::u16string& country_code = profile_.GetInfo(kCountryCode, locale);

  std::vector<std::vector<::i18n::addressinput::AddressUiComponent>> components;
  autofill::GetAddressComponents(base::UTF16ToUTF8(country_code), locale,
                                 &components, nullptr);

  std::vector<std::u16string> address_lines;
  for (const std::vector<::i18n::addressinput::AddressUiComponent>& line :
       components) {
    std::vector<std::u16string> line_components;
    for (const ::i18n::addressinput::AddressUiComponent& component : line) {
      std::u16string component_str = profile_.GetInfo(
          autofill::AddressFieldToServerFieldType(component.field), locale);
      if (!component_str.empty())
        line_components.push_back(component_str);
    }
    if (!line_components.empty())
      address_lines.push_back(base::JoinString(line_components, u" "));
  }
  std::u16string country = profile_.GetInfo(ADDRESS_HOME_COUNTRY, locale);
  if (!country.empty())
    address_lines.push_back(country);
  return base::JoinString(address_lines, u"\n");
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
