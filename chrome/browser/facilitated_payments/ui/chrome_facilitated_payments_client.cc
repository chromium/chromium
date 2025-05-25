// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/android/device_delegate_android.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/network_api/multiple_request_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

ChromeFacilitatedPaymentsClient::ChromeFacilitatedPaymentsClient(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsUserData<ChromeFacilitatedPaymentsClient>(
          *web_contents),
      driver_factory_(web_contents,
                      /* client= */ this),
      facilitated_payments_controller_(
          std::make_unique<FacilitatedPaymentsController>(web_contents)),
      optimization_guide_decider_(optimization_guide_decider) {
  RegisterAllowlists();
}

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

payments::facilitated::MultipleRequestFacilitatedPaymentsNetworkInterface*
ChromeFacilitatedPaymentsClient::
    GetMultipleRequestFacilitatedPaymentsNetworkInterface() {
  if (!multiple_request_facilitated_payments_network_interface_) {
    Profile* profile =
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
    if (!profile) {
      return nullptr;
    }
    multiple_request_facilitated_payments_network_interface_ = std::make_unique<
        payments::facilitated::
            MultipleRequestFacilitatedPaymentsNetworkInterface>(
        profile->GetURLLoaderFactory(),
        *IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
        *GetPaymentsDataManager(), profile->IsOffTheRecord());
  }
  return multiple_request_facilitated_payments_network_interface_.get();
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

bool ChromeFacilitatedPaymentsClient::IsFoldable() {
  return base::android::BuildInfo::GetInstance()->is_foldable();
}

optimization_guide::OptimizationGuideDecider*
ChromeFacilitatedPaymentsClient::GetOptimizationGuideDecider() {
  return optimization_guide_decider_;
}

void ChromeFacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {
  facilitated_payments_controller_->Show(
      std::move(bank_account_suggestions),
      std::move(on_payment_account_selected));
}

void ChromeFacilitatedPaymentsClient::ShowEwalletPaymentPrompt(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {
  facilitated_payments_controller_->ShowForEwallet(
      ewallet_suggestions, std::move(on_payment_account_selected));
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

void ChromeFacilitatedPaymentsClient::SetUiEventListener(
    base::RepeatingCallback<void(payments::facilitated::UiEvent)>
        ui_event_listener) {
  facilitated_payments_controller_->SetUiEventListener(
      std::move(ui_event_listener));
}

payments::facilitated::ContentFacilitatedPaymentsDriver*
ChromeFacilitatedPaymentsClient::GetFacilitatedPaymentsDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  return &driver_factory_.GetOrCreateForFrame(render_frame_host);
}

autofill::StrikeDatabase* ChromeFacilitatedPaymentsClient::GetStrikeDatabase() {
  content::BrowserContext* context = GetWebContents().GetBrowserContext();

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  return autofill::StrikeDatabaseFactory::GetForProfile(profile);
}

bool ChromeFacilitatedPaymentsClient::IsPixAccountLinkingSupported() const {
  return payments::facilitated::IsWalletEligibleForPixAccountLinking();
}

void ChromeFacilitatedPaymentsClient::ShowPixAccountLinkingPrompt() {
  facilitated_payments_controller_->ShowPixAccountLinkingPrompt();
}

void ChromeFacilitatedPaymentsClient::RegisterAllowlists() {
  if (optimization_guide_decider_) {
    if (base::FeatureList::IsEnabled(payments::facilitated::kEwalletPayments)) {
      optimization_guide_decider_->RegisterOptimizationTypes(
          {optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST});
    }
    if (base::FeatureList::IsEnabled(
            payments::facilitated::kEnablePixPayments)) {
      optimization_guide_decider_->RegisterOptimizationTypes(
          {optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST});
    }
  }
}

void ChromeFacilitatedPaymentsClient::
    SetFacilitatedPaymentsControllerForTesting(
        std::unique_ptr<FacilitatedPaymentsController> mock_controller) {
  facilitated_payments_controller_ = std::move(mock_controller);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeFacilitatedPaymentsClient);
