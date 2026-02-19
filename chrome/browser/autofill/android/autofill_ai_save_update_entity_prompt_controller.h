// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillAiSaveUpdateEntityPromptView;

// Android implementation of the modal prompt for saving new/updating existing
// address profile. The class is responsible for showing the view and handling
// user interactions. The controller owns its java counterpart and the
// corresponding view.
class AutofillAiSaveUpdateEntityPromptController {
 public:
  AutofillAiSaveUpdateEntityPromptController(
      content::WebContents* web_contents,
      std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view,
      EntityInstance entity_instance,
      std::optional<EntityInstance> old_entity_instance,
      std::string app_locale,
      AutofillClient::EntityImportPromptResultCallback prompt_result_callback);
  AutofillAiSaveUpdateEntityPromptController(
      const AutofillAiSaveUpdateEntityPromptController&) = delete;
  AutofillAiSaveUpdateEntityPromptController& operator=(
      const AutofillAiSaveUpdateEntityPromptController&) = delete;
  ~AutofillAiSaveUpdateEntityPromptController();

  void DisplayPrompt();

  std::u16string GetTitle() const;
  std::u16string GetPositiveButtonText() const;
  std::u16string GetNegativeButtonText() const;

  std::vector<EntityAttributeUpdateDetails> GetEntityUpdateDetails() const;

  std::u16string GetSourceNotice() const;
  // Returns true if the entity to be saved or updated will be stored in the
  // wallet server.
  bool IsWalletableEntity() const;
  bool IsUpdatePrompt() const;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;
  // Called by AutofillAiSaveUpdateEntityPromptController.java
  void OpenManagePasses(JNIEnv* env);
  void OnUserAccepted(JNIEnv* env);
  void OnUserDeclined(JNIEnv* env);
  // Called whenever the prompt is dismissed (e.g. because the user already
  // accepted/declined/edited the entity (after OnUserAccepted/Declined/Edited
  // is called) or it was closed without interaction).
  void OnPromptDismissed(JNIEnv* env);

 private:
  void RunPromptClosedCallback(AutofillClient::AutofillAiBubbleResult result);

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AutofillAiSaveUpdateEntityPromptView> prompt_view_;
  const EntityInstance entity_instance_;
  const std::optional<EntityInstance> old_entity_instance_;
  const std::string app_locale_;
  // If the user explicitly accepted/dismissed/edited the entity.
  bool had_user_interaction_ = false;
  // The callback to run when the user takes action on the prompt.
  AutofillClient::EntityImportPromptResultCallback prompt_result_callback_;
  // The corresponding Java SaveUpdateAddressProfilePromptController.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER_H_
