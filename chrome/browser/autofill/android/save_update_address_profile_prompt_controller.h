// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_

#include <jni.h>

#include <memory>
#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_view.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"

namespace signin {
class IdentityManager;
}

namespace autofill {

class PersonalDataManager;

// Android implementation of the modal prompt for saving new/updating existing
// address profile. The class is responsible for showing the view and handling
// user interactions. The controller owns its java counterpart and the
// corresponding view.
class SaveUpdateAddressProfilePromptController {
 public:
  SaveUpdateAddressProfilePromptController(
      std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view,
      autofill::PersonalDataManager* personal_data,
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback decision_callback,
      base::OnceCallback<void()> dismissal_callback);
  SaveUpdateAddressProfilePromptController(
      const SaveUpdateAddressProfilePromptController&) = delete;
  SaveUpdateAddressProfilePromptController& operator=(
      const SaveUpdateAddressProfilePromptController&) = delete;
  ~SaveUpdateAddressProfilePromptController();

  void DisplayPrompt();

  std::u16string GetTitle();
  std::u16string GetRecordTypeNotice(signin::IdentityManager* profile);
  std::u16string GetPositiveButtonText();
  std::u16string GetNegativeButtonText();
  // For save prompt:
  std::u16string GetAddress();
  std::u16string GetEmail();
  std::u16string GetPhoneNumber();
  // For update prompt:
  std::u16string GetSubtitle();
  // Returns two strings listing formatted profile data that will change when
  // the `original_profile_` is updated to `profile_`. The old values, which
  // will be replaced, are the first value, and the new values, which will be
  // saved, are the second value.
  std::pair<std::u16string, std::u16string> GetDiffFromOldToNewProfile();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void OnUserAccepted(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnUserDeclined(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnUserEdited(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& jprofile);
  // Called whenever the prompt is dismissed (e.g. because the user already
  // accepted/declined/edited the profile (after OnUserAccepted/Declined/Edited
  // is called) or it was closed without interaction).
  void OnPromptDismissed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  void RunSaveAddressProfileCallback(
      AutofillClient::AddressPromptUserDecision decision);

  // If the user explicitly accepted/dismissed/edited the profile.
  bool had_user_interaction_ = false;
  // View that displays the prompt.
  std::unique_ptr<SaveUpdateAddressProfilePromptView> prompt_view_;
  // The personal data manager associated with the current web contents of the
  // tab the prompt will be displayed in. The lifetime of this object is
  // constrained with the lifetime of the tab's web content, which owns the
  // corresponding personal data manager.
  raw_ptr<autofill::PersonalDataManager> personal_data_;
  // The profile which is being confirmed by the user.
  AutofillProfile profile_;
  // The profile (if exists) which will be updated if the user confirms.
  std::optional<AutofillProfile> original_profile_;
  // The option which specifies whether the autofill profile is going to be
  // migrated to user's Google Account.
  bool is_migration_to_account_;
  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback decision_callback_;
  // The callback guaranteed to be run once the prompt is dismissed.
  base::OnceCallback<void()> dismissal_callback_;
  // The corresponding Java SaveUpdateAddressProfilePromptController.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
