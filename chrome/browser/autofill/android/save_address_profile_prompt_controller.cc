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
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfilePromptController::SaveAddressProfilePromptController(
    std::unique_ptr<SaveAddressProfilePromptView> prompt_view,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback decision_callback,
    base::OnceCallback<void()> dismissal_callback)
    : prompt_view_(std::move(prompt_view)),
      profile_(profile),
      original_profile_(base::OptionalFromPtr(original_profile)),
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
  bool success =
      prompt_view_->Show(this, profile_, /*is_update=*/!!original_profile_);
  if (!success)
    std::move(dismissal_callback_).Run();
}

std::u16string SaveAddressProfilePromptController::GetTitle() {
  // TODO(crbug.com/1167061): Replace with proper localized strings.
  // TODO(crbug.com/1167061): Make update title reflect fields to be updated.
  return original_profile_ ? u"Update address?" : u"Save address?";
}

std::u16string SaveAddressProfilePromptController::GetPositiveButtonText() {
  // TODO(crbug.com/1167061): Replace with proper localized strings.
  return original_profile_ ? u"Update" : u"Save";
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

std::u16string SaveAddressProfilePromptController::GetSubtitle() {
  DCHECK(original_profile_);
  return u"For " + GetDescriptionForProfileToUpdate(
                       original_profile_.value(),
                       g_browser_process->GetApplicationLocale());
}

std::pair<std::u16string, std::u16string>
SaveAddressProfilePromptController::GetDiffFromOldToNewProfile() {
  DCHECK(original_profile_);
  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
      differences =
          AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
              original_profile_.value(), profile_,
              g_browser_process->GetApplicationLocale());
  std::vector<std::u16string> old_values;
  std::vector<std::u16string> new_values;
  for (auto type : kVisibleTypesForProfileDifferences) {
    auto it = differences.find(type);
    if (it == differences.end())
      continue;
    if (!it->second.first.empty())
      old_values.push_back(it->second.first);
    if (!it->second.second.empty())
      new_values.push_back(it->second.second);
  }
  return std::make_pair(base::JoinString(old_values, u"\n"),
                        base::JoinString(new_values, u"\n"));
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
      was_profile_edited
          ? AutofillClient::SaveAddressProfileOfferUserDecision::kEdited
          : AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

void SaveAddressProfilePromptController::OnUserDeclined(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

void SaveAddressProfilePromptController::OnUserEdited(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  was_profile_edited = true;
  AutofillProfile edited_profile;
  PersonalDataManagerAndroid::PopulateNativeProfileFromJava(jprofile, env,
                                                            &edited_profile);
  profile_ = edited_profile;
  prompt_view_->RefreshContent();
}

void SaveAddressProfilePromptController::OnPromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::move(dismissal_callback_).Run();
}

void SaveAddressProfilePromptController::RunSaveAddressProfileCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(decision_callback_).Run(decision, profile_);
}

}  // namespace autofill
