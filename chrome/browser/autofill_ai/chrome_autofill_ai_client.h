// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_

#include <memory>
#include <optional>

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

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

  // Creates a `ChromeAutofillAiClient` for `web_contents` if
  // `kAutofillAiWithDataSchema` is enabled. Returns `nullptr` otherwise.
  [[nodiscard]] static std::unique_ptr<ChromeAutofillAiClient>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // AutofillAiClient:
  autofill::ContentAutofillClient& GetAutofillClient() override;
  autofill_ai::AutofillAiManager& GetManager() override;
  autofill::EntityDataManager* GetEntityDataManager() override;
  autofill::FormStructure* GetCachedFormStructure(
      const autofill::FormGlobalId& form_id) override;
  optimization_guide::ModelQualityLogsUploaderService* GetMqlsUploadService()
      override;
  void ShowSaveOrUpdateBubble(
      autofill::EntityInstance new_entity,
      std::optional<autofill::EntityInstance> old_entity,
      SaveOrUpdatePromptResultCallback save_prompt_acceptance_callback)
      override;

 private:
  explicit ChromeAutofillAiClient(content::WebContents* web_contents);

  const raw_ref<content::WebContents> web_contents_;

  autofill_ai::AutofillAiManager prediction_improvements_manager_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
