// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

namespace autofill {

class AutofillAiSaveUpdateEntityPromptView;

// Android implementation of the modal prompt for saving new/updating existing
// address profile. The class is responsible for showing the view and handling
// user interactions. The controller owns its java counterpart and the
// corresponding view.
// TODO: crbug.com/460410690 - Write tests.
class AutofillAiSaveUpdateEntityPromptController {
 public:
  AutofillAiSaveUpdateEntityPromptController(
      std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view,
      const EntityTypeName entity_type_name);
  AutofillAiSaveUpdateEntityPromptController(
      const AutofillAiSaveUpdateEntityPromptController&) = delete;
  AutofillAiSaveUpdateEntityPromptController& operator=(
      const AutofillAiSaveUpdateEntityPromptController&) = delete;
  ~AutofillAiSaveUpdateEntityPromptController();

  void DisplayPrompt();

  std::u16string GetTitle() const;
  std::u16string GetPositiveButtonText() const;
  std::u16string GetNegativeButtonText() const;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;
  void OnUserAccepted(JNIEnv* env);
  void OnUserDeclined(JNIEnv* env);
  // Called whenever the prompt is dismissed (e.g. because the user already
  // accepted/declined/edited the entity (after OnUserAccepted/Declined/Edited
  // is called) or it was closed without interaction).
  void OnPromptDismissed(JNIEnv* env);

 private:
  std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view_;
  const EntityTypeName entity_type_name_;
  // If the user explicitly accepted/dismissed/edited the entity.
  bool had_user_interaction_ = false;
  // The corresponding Java SaveUpdateAddressProfilePromptController.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_
