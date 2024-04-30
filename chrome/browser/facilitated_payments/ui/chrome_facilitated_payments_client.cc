// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "content/public/browser/web_contents.h"

ChromeFacilitatedPaymentsClient::ChromeFacilitatedPaymentsClient(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsUserData<ChromeFacilitatedPaymentsClient>(
          *web_contents),
      driver_factory_(web_contents,
                      /*client=*/this,
                      optimization_guide_decider) {}

ChromeFacilitatedPaymentsClient::~ChromeFacilitatedPaymentsClient() = default;

void ChromeFacilitatedPaymentsClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> on_risk_data_loaded_callback) {
  autofill::risk_util::LoadRiskData(/*obfuscated_gaia_id=*/0, &GetWebContents(),
                                    std::move(on_risk_data_loaded_callback));
}

autofill::PersonalDataManager*
ChromeFacilitatedPaymentsClient::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (profile) {
    return autofill::PersonalDataManagerFactory::GetForProfile(profile);
  }
  return nullptr;
}

bool ChromeFacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeFacilitatedPaymentsClient);
