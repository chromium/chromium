// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SaveUpdateAddressProfilePromptController_jni.h"

namespace autofill {

SaveUpdateAddressProfilePromptController::
    SaveUpdateAddressProfilePromptController(
        std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view,
        autofill::PersonalDataManager* personal_data,
        const AutofillProfile& profile,
        const AutofillProfile* original_profile,
        bool is_migration_to_account,
        AutofillClient::AddressProfileSavePromptCallback decision_callback,
        base::OnceCallback<void()> dismissal_callback)
    : prompt_view_(std::move(prompt_view)),
      personal_data_(personal_data),
      profile_(profile),
      original_profile_(base::OptionalFromPtr(original_profile)),
      is_migration_to_account_(is_migration_to_account),
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
        AutofillClient::AddressPromptUserDecision::kIgnored);
  }
}

void SaveUpdateAddressProfilePromptController::DisplayPrompt() {
  bool success =
      prompt_view_->Show(this, profile_, /*is_update=*/!!original_profile_,
                         is_migration_to_account_);
  if (!success)
    std::move(dismissal_callback_).Run();
}

std::u16string SaveUpdateAddressProfilePromptController::GetTitle() {
  if (original_profile_) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  }

  return l10n_util::GetStringUTF16(
      is_migration_to_account_
          ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
          : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
}

std::u16string SaveUpdateAddressProfilePromptController::GetRecordTypeNotice(
    signin::IdentityManager* identity_manager) {
  if (!is_migration_to_account_ && !profile_.IsAccountProfile()) {
    return std::u16string();
  }
  std::optional<AccountInfo> account =
      identity_manager->FindExtendedAccountInfo(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
  if (!account) {
    return std::u16string();
  }

  // Notify user that their address is saved only in Chrome and can be migrated
  // to their Google account.
  if (is_migration_to_account_) {
    // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync is not used
    // anymore, and thus IsSyncFeatureEnabledForAutofill() will always be false.
    return l10n_util::GetStringFUTF16(
        personal_data_->address_data_manager().IsSyncFeatureEnabledForAutofill()
            ? IDS_AUTOFILL_SYNCABLE_PROFILE_MIGRATION_PROMPT_NOTICE
            : IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  // Notify user that their address has already been saved in their Google
  // account and is only going to be updated there.
  if (original_profile_) {
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_ADDRESS_ALREADY_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  // Notify the user that their address is going to be saved in their Google
  // account if they accept the prompt.
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ADDRESS_WILL_BE_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE,
      base::UTF8ToUTF16(account->email));
}

std::u16string
SaveUpdateAddressProfilePromptController::GetPositiveButtonText() {
  if (original_profile_) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF16(
      is_migration_to_account_
          ? IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_MIGRATION_OK_BUTTON_LABEL
          : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

std::u16string
SaveUpdateAddressProfilePromptController::GetNegativeButtonText() {
  if (is_migration_to_account_) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_MIGRATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF16(
      IDS_ANDROID_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL);
}

std::u16string SaveUpdateAddressProfilePromptController::GetAddress() {
  if (is_migration_to_account_) {
    const std::u16string name =
        profile_.GetInfo(NAME_FULL, g_browser_process->GetApplicationLocale());
    const std::u16string address = profile_.GetInfo(
        ADDRESS_HOME_LINE1, g_browser_process->GetApplicationLocale());
    const std::u16string separator =
        !name.empty() && !address.empty() ? u"\n" : u"";
    return base::StrCat({name, separator, address});
  } else {
    return GetEnvelopeStyleAddress(profile_,
                                   g_browser_process->GetApplicationLocale(),
                                   /*include_recipient=*/true,
                                   /*include_country=*/true);
  }
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
      AutofillClient::AddressPromptUserDecision::kAccepted);
}

void SaveUpdateAddressProfilePromptController::OnUserDeclined(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  had_user_interaction_ = true;
  RunSaveAddressProfileCallback(
      is_migration_to_account_
          ? AutofillClient::AddressPromptUserDecision::kNever
          : AutofillClient::AddressPromptUserDecision::kDeclined);
}

void SaveUpdateAddressProfilePromptController::OnUserEdited(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  had_user_interaction_ = true;
  AutofillProfile* existing_profile =
      original_profile_.has_value() ? &original_profile_.value() : nullptr;
  AutofillProfile edited_profile = AutofillProfile::CreateFromJavaObject(
      jprofile, existing_profile, g_browser_process->GetApplicationLocale());
  profile_ = edited_profile;
  RunSaveAddressProfileCallback(
      AutofillClient::AddressPromptUserDecision::kEditAccepted);
}

void SaveUpdateAddressProfilePromptController::OnPromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::move(dismissal_callback_).Run();
}

void SaveUpdateAddressProfilePromptController::RunSaveAddressProfileCallback(
    AutofillClient::AddressPromptUserDecision decision) {
  std::move(decision_callback_)
      .Run(decision,
           decision == AutofillClient::AddressPromptUserDecision::kEditAccepted
               ? base::optional_ref(profile_)
               : std::nullopt);
}

}  // namespace autofill
