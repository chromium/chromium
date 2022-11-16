// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "chrome/android/chrome_jni_headers/SaveUpdateAddressProfilePromptController_jni.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveUpdateAddressProfilePromptController::
    SaveUpdateAddressProfilePromptController(
        std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view,
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
}

SaveUpdateAddressProfilePromptController::
    ~SaveUpdateAddressProfilePromptController() {
  if (java_object_) {
    Java_SaveUpdateAddressProfilePromptController_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
  if (!had_user_interaction_) {
    RunSaveAddressProfileCallback(
        AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored);
  }
}

void SaveUpdateAddressProfilePromptController::DisplayPrompt() {
  bool success =
      prompt_view_->Show(this, profile_, /*is_update=*/!!original_profile_);
  if (!success)
    std::move(dismissal_callback_).Run();
}

std::u16string SaveUpdateAddressProfilePromptController::GetTitle() {
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE
                        : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
}

std::u16string
SaveUpdateAddressProfilePromptController::GetPositiveButtonText() {
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
                        : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

std::u16string
SaveUpdateAddressProfilePromptController::GetNegativeButtonText() {
  return l10n_util::GetStringUTF16(
      IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL);
}

std::u16string SaveUpdateAddressProfilePromptController::GetAddress() {
  return GetEnvelopeStyleAddress(profile_,
                                 g_browser_process->GetApplicationLocale(),
                                 /*include_recipient=*/true,
                                 /*include_country=*/true);
}

std::u16string SaveUpdateAddressProfilePromptController::GetEmail() {
  return profile_.GetInfo(EMAIL_ADDRESS,
                          g_browser_process->GetApplicationLocale());
}

std::u16string SaveUpdateAddressProfilePromptController::GetPhoneNumber() {
  return profile_.GetInfo(PHONE_HOME_WHOLE_NUMBER,
                          g_browser_process->GetApplicationLocale());
}

std::u16string SaveUpdateAddressProfilePromptController::GetSubtitle() {
  DCHECK(original_profile_);
  const std::string locale = g_browser_process->GetApplicationLocale();
  std::vector<ProfileValueDifference> differences =
      GetProfileDifferenceForUi(original_profile_.value(), profile_, locale);
  bool address_updated = base::Contains(differences, ADDRESS_HOME_ADDRESS,
                                        &ProfileValueDifference::type);
  return GetProfileDescription(
      original_profile_.value(), locale,
      /*include_address_and_contacts=*/!address_updated);
}

std::pair<std::u16string, std::u16string>
SaveUpdateAddressProfilePromptController::GetDiffFromOldToNewProfile() {
  DCHECK(original_profile_);
  std::vector<ProfileValueDifference> differences =
      GetProfileDifferenceForUi(original_profile_.value(), profile_,
                                g_browser_process->GetApplicationLocale());

  std::u16string old_diff;
  std::u16string new_diff;
  for (const auto& diff : differences) {
    if (!diff.first_value.empty()) {
      old_diff += diff.first_value + u"\n";
      // Add an extra newline to separate address and the following contacts.
      if (diff.type == ADDRESS_HOME_ADDRESS)
        old_diff += u"\n";
    }
    if (!diff.second_value.empty()) {
      new_diff += diff.second_value + u"\n";
      // Add an extra newline to separate address and the following contacts.
      if (diff.type == ADDRESS_HOME_ADDRESS)
        new_diff += u"\n";
    }
  }
  // Make sure there will be no newlines in the end.
  base::TrimString(old_diff, base::kWhitespaceASCIIAs16, &old_diff);
  base::TrimString(new_diff, base::kWhitespaceASCIIAs16, &new_diff);
  return std::make_pair(std::move(old_diff), std::move(new_diff));
}

base::android::ScopedJavaLocalRef<jobject>
SaveUpdateAddressProfilePromptController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_SaveUpdateAddressProfilePromptController_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void SaveUpdateAddressProfilePromptController::OnUserAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

void SaveUpdateAddressProfilePromptController::OnUserDeclined(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

void SaveUpdateAddressProfilePromptController::OnUserEdited(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  had_user_interaction_ = true;
  AutofillProfile edited_profile;
  PersonalDataManagerAndroid::PopulateNativeProfileFromJava(jprofile, env,
                                                            &edited_profile);
  profile_ = edited_profile;
  RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted);
}

void SaveUpdateAddressProfilePromptController::OnPromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::move(dismissal_callback_).Run();
}

void SaveUpdateAddressProfilePromptController::RunSaveAddressProfileCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(decision_callback_).Run(decision, profile_);
}

}  // namespace autofill
