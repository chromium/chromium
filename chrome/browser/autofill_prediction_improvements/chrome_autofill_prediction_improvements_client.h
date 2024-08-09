// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
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

  // Returns the page context. Which is a string summary of it.
  void GetPageContext(
      autofill_prediction_improvements::AutofillPredictionImprovementsClient::
          PageContextCallback callback) override;
  // Returns the Prediction Improvements manager.
  autofill_prediction_improvements::AutofillPredictionImprovementsManager&
  GetManager() override;

 protected:
  explicit ChromeAutofillPredictionImprovementsClient(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      ChromeAutofillPredictionImprovementsClient>;

  autofill_prediction_improvements::AutofillPredictionImprovementsManager
      manager_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_CHROME_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
