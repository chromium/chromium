// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

ChromeAutofillPredictionImprovementsClient::
    ChromeAutofillPredictionImprovementsClient(
        content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeAutofillPredictionImprovementsClient>(
          *web_contents) {}

ChromeAutofillPredictionImprovementsClient::
    ~ChromeAutofillPredictionImprovementsClient() = default;

void ChromeAutofillPredictionImprovementsClient::GetPageContext(
    autofill_prediction_improvements::AutofillPredictionImprovementsClient::
        PageContextCallback callback) {}

autofill_prediction_improvements::AutofillPredictionImprovementsManager&
ChromeAutofillPredictionImprovementsClient::GetManager() {
  return manager_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAutofillPredictionImprovementsClient);
