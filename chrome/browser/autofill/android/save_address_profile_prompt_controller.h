// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_

#include <memory>

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "chrome/browser/autofill/android/save_address_profile_prompt_view.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Android implementation of the modal prompt for saving an address profile.
// The class is responsible for showing the view and handling user
// interactions. The controller owns its java counterpart and the corresponding
// view.
class SaveAddressProfilePromptController {
 public:
  SaveAddressProfilePromptController(
      std::unique_ptr<SaveAddressProfilePromptView> prompt_view,
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback decision_callback,
      base::OnceCallback<void()> dismissal_callback);
  SaveAddressProfilePromptController(
      const SaveAddressProfilePromptController&) = delete;
  SaveAddressProfilePromptController& operator=(
      const SaveAddressProfilePromptController&) = delete;
  ~SaveAddressProfilePromptController();

  void DisplayPrompt();

  void OnAccepted();
  void OnDeclined();
  // Called whenever the prompt is dismissed (e.g. because the user already
  // accepted/declined the prompt (after OnAccepted/OnDeclined is called) or
  // it was closed without interaction).
  void OnPromptDismissed();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void OnUserAccepted(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnUserDeclined(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnPromptDismissed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  void RunSaveAddressProfileCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision decision);

  // If the user explicitly accepted/dismissed the prompt.
  bool had_user_interaction_ = false;
  // View that displays the prompt.
  std::unique_ptr<SaveAddressProfilePromptView> prompt_view_;
  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;
  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback decision_callback_;
  // The callback guaranteed to be run once the prompt is dismissed.
  base::OnceCallback<void()> dismissal_callback_;
  // The corresponding Java SaveAddressProfilePromptController.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_SAVE_ADDRESS_PROFILE_PROMPT_CONTROLLER_H_
