// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

ChromeFacilitatedPaymentsClient::ChromeFacilitatedPaymentsClient(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsUserData<ChromeFacilitatedPaymentsClient>(
          *web_contents),
      driver_factory_(web_contents,
                      /*client=*/this,
                      optimization_guide_decider),
      facilitated_payments_controller_(
          std::make_unique<FacilitatedPaymentsController>(web_contents)) {}

ChromeFacilitatedPaymentsClient::~ChromeFacilitatedPaymentsClient() = default;

void ChromeFacilitatedPaymentsClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> on_risk_data_loaded_callback) {
  autofill::risk_util::LoadRiskData(/*obfuscated_gaia_id=*/0, &GetWebContents(),
                                    std::move(on_risk_data_loaded_callback));
}

autofill::PaymentsDataManager*
ChromeFacilitatedPaymentsClient::GetPaymentsDataManager() {
  content::BrowserContext* context = GetWebContents().GetBrowserContext();
  if (!context) {
    return nullptr;
  }
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(context);
  return pdm ? &pdm->payments_data_manager() : nullptr;
}

payments::facilitated::FacilitatedPaymentsNetworkInterface*
ChromeFacilitatedPaymentsClient::GetFacilitatedPaymentsNetworkInterface() {
  if (!facilitated_payments_network_interface_) {
    Profile* profile =
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
    if (!profile) {
      return nullptr;
    }
    facilitated_payments_network_interface_ = std::make_unique<
        payments::facilitated::FacilitatedPaymentsNetworkInterface>(
        profile->GetURLLoaderFactory(),
        IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
        GetPaymentsDataManager(), profile->IsOffTheRecord());
  }
  return facilitated_payments_network_interface_.get();
}

std::optional<CoreAccountInfo>
ChromeFacilitatedPaymentsClient::GetCoreAccountInfo() {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (!profile) {
    return std::nullopt;
  }
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
}

bool ChromeFacilitatedPaymentsClient::IsInLandscapeMode() {
  return facilitated_payments_controller_->IsInLandscapeMode();
}

bool ChromeFacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  return facilitated_payments_controller_->Show(
      std::move(bank_account_suggestions),
      std::move(on_user_decision_callback));
}

void ChromeFacilitatedPaymentsClient::ShowProgressScreen() {
  facilitated_payments_controller_->ShowProgressScreen();
}

void ChromeFacilitatedPaymentsClient::ShowErrorScreen() {
  facilitated_payments_controller_->ShowErrorScreen();
}

void ChromeFacilitatedPaymentsClient::DismissPrompt() {
  facilitated_payments_controller_->Dismiss();
}

payments::facilitated::ContentFacilitatedPaymentsDriver*
ChromeFacilitatedPaymentsClient::GetFacilitatedPaymentsDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  return &driver_factory_.GetOrCreateForFrame(render_frame_host);
}

void ChromeFacilitatedPaymentsClient::
    SetFacilitatedPaymentsControllerForTesting(
        std::unique_ptr<FacilitatedPaymentsController> mock_controller) {
  facilitated_payments_controller_ = std::move(mock_controller);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeFacilitatedPaymentsClient);
