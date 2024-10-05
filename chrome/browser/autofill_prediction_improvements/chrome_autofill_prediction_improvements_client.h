// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// An implementation of `AutofillPredictionImprovementsClient` for Desktop and
// Android.
class ChromeAutofillPredictionImprovementsClient
    : public autofill_prediction_improvements::
          AutofillPredictionImprovementsClient,
      public content::WebContentsUserData<
          ChromeAutofillPredictionImprovementsClient> {
 public:
  ChromeAutofillPredictionImprovementsClient(
      const ChromeAutofillPredictionImprovementsClient&) = delete;
  ChromeAutofillPredictionImprovementsClient& operator=(
      const ChromeAutofillPredictionImprovementsClient&) = delete;
  ~ChromeAutofillPredictionImprovementsClient() override;

  // Creates a `ChromeAutofillPredictionImprovementsClient` for `web_contents`
  // if allowed.
  static std::unique_ptr<ChromeAutofillPredictionImprovementsClient>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // AutofillPredictionImprovementsClient:
  void GetAXTree(AXTreeCallback callback) override;
  autofill_prediction_improvements::AutofillPredictionImprovementsManager&
  GetManager() override;
  autofill_prediction_improvements::AutofillPredictionImprovementsFillingEngine*
  GetFillingEngine() override;
  const GURL& GetLastCommittedURL() override;
  std::string GetTitle() override;
  user_annotations::UserAnnotationsService* GetUserAnnotationsService()
      override;
  bool IsAutofillPredictionImprovementsEnabledPref() const override;
  void TryToOpenFeedbackPage(const std::string& feedback_id) override;
  void OpenPredictionImprovementsSettings() override;
  bool IsUserEligible() override;

 protected:
  explicit ChromeAutofillPredictionImprovementsClient(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      ChromeAutofillPredictionImprovementsClient>;

  const raw_ref<const PrefService> prefs_;

  // Returns whether the optimization guide suggests that Autofill prediction
  // improvements should currently be allowed to report feedback.
  bool CanShowFeedbackPage();

  std::unique_ptr<autofill_prediction_improvements::
                      AutofillPredictionImprovementsFillingEngine>
      filling_engine_;

  autofill_prediction_improvements::AutofillPredictionImprovementsManager
      prediction_improvements_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
