// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
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
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  std::string GetTitle() override;
  user_annotations::UserAnnotationsService* GetUserAnnotationsService()
      override;
  bool IsAutofillAiEnabledPref() const override;
  void TryToOpenFeedbackPage(const std::string& feedback_id) override;
  void OpenPredictionImprovementsSettings() override;
  bool IsUserEligible() override;
  autofill::FormStructure* GetCachedFormStructure(
      const autofill::FormData& form_data) override;
  std::u16string GetAutofillNameFillingValue(
      const std::string& autofill_profile_guid,
      autofill::FieldType field_type,
      const autofill::FormFieldData& field) override;
  void ShowSaveAutofillAiBubble(
      std::unique_ptr<user_annotations::FormAnnotationResponse>
          form_annotation_response,
      user_annotations::PromptAcceptanceCallback prompt_acceptance_callback)
      override;

 private:
  explicit ChromeAutofillAiClient(content::WebContents* web_contents,
                                  Profile* profile);

  const raw_ref<content::WebContents> web_contents_;
  const raw_ref<const PrefService> prefs_;

  // Returns whether the optimization guide suggests that Autofill prediction
  // improvements should currently be allowed to report feedback.
  bool CanShowFeedbackPage();

  std::unique_ptr<autofill_ai::AutofillAiModelExecutor> filling_engine_;

  autofill_ai::AutofillAiManager prediction_improvements_manager_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AI_CHROME_AUTOFILL_AI_CLIENT_H_
