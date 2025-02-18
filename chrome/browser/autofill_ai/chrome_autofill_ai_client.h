// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_

#include <memory>
#include <utility>

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/prefs/pref_service.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// An implementation of `AutofillAiClient` for Desktop and
// Android.
class ChromeAutofillAiClient : public autofill_ai::AutofillAiClient {
 public:
  ChromeAutofillAiClient(const ChromeAutofillAiClient&) = delete;
  ChromeAutofillAiClient& operator=(const ChromeAutofillAiClient&) = delete;
  ~ChromeAutofillAiClient() override;

  // Creates a `ChromeAutofillAiClient` for `web_contents`
  // if it is supported, i.e., `autofill_ai::
  // IsAutofillAiSupported()` is true.
  [[nodiscard]] static std::unique_ptr<ChromeAutofillAiClient>
  MaybeCreateForWebContents(content::WebContents* web_contents,
                            Profile* profile);

  // AutofillAiClient:
  autofill::ContentAutofillClient& GetAutofillClient() override;
  void GetAXTree(AXTreeCallback callback) override;
  autofill_ai::AutofillAiManager& GetManager() override;
  autofill_ai::AutofillAiModelExecutor* GetModelExecutor() override;
  autofill::EntityDataManager* GetEntityDataManager() override;
  bool IsAutofillAiEnabledPref() const override;
  bool IsUserEligible() override;
  autofill::FormStructure* GetCachedFormStructure(
      const autofill::FormGlobalId& form_id) override;
  void ShowSaveAutofillAiBubble(
      autofill::EntityInstance new_entity,
      std::optional<autofill::EntityInstance> old_entity,
      SavePromptAcceptanceCallback save_prompt_acceptance_callback) override;

  void SetModelExecutorForTesting(
      std::unique_ptr<autofill_ai::AutofillAiModelExecutor> model_executor) {
    filling_engine_ = std::move(model_executor);
  }

 private:
  explicit ChromeAutofillAiClient(content::WebContents* web_contents,
                                  Profile* profile);

  const raw_ref<content::WebContents> web_contents_;
  const raw_ref<const PrefService> prefs_;

  // Returns whether the optimization guide suggests that Autofill AI should
  // should currently be allowed to report feedback.
  bool CanShowFeedbackPage();

  // TODO(crbug.com/371534239): Rename to `model_executor_`.
  std::unique_ptr<autofill_ai::AutofillAiModelExecutor> filling_engine_;

  autofill_ai::AutofillAiManager prediction_improvements_manager_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
