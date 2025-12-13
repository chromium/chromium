// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/android/device_info.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/android/device_delegate_android.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

ChromeFacilitatedPaymentsClient::ChromeFacilitatedPaymentsClient(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsUserData<ChromeFacilitatedPaymentsClient>(
          *web_contents),
      driver_factory_(web_contents,
                      /* client= */ this),
      facilitated_payments_controller_(
          std::make_unique<FacilitatedPaymentsController>(web_contents)),
      optimization_guide_decider_(optimization_guide_decider),
      device_delegate_(web_contents) {
  RegisterAllowlists();
}

ChromeFacilitatedPaymentsClient::~ChromeFacilitatedPaymentsClient() = default;

void ChromeFacilitatedPaymentsClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> on_risk_data_loaded_callback) {
  autofill::risk_util::LoadRiskData(/*obfuscated_gaia_id=*/0, &GetWebContents(),
                                    std::move(on_risk_data_loaded_callback));
}

const url::Origin& ChromeFacilitatedPaymentsClient::GetLastCommittedOrigin()
    const {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
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
        *IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
        *GetPaymentsDataManager(), profile->IsOffTheRecord());
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

bool ChromeFacilitatedPaymentsClient::IsFoldable() {
  return base::android::device_info::is_foldable();
}

bool ChromeFacilitatedPaymentsClient::IsInChromeCustomTabMode() {
  auto* delegate = TabAndroid::FromWebContents(&GetWebContents())
                       ? static_cast<android::TabWebContentsDelegateAndroid*>(
                             GetWebContents().GetDelegate())
                       : nullptr;
  return delegate && delegate->IsCustomTab();
}

optimization_guide::OptimizationGuideDecider*
ChromeFacilitatedPaymentsClient::GetOptimizationGuideDecider() {
  return optimization_guide_decider_;
}

payments::facilitated::DeviceDelegate*
ChromeFacilitatedPaymentsClient::GetDeviceDelegate() {
  return &device_delegate_;
}

bool ChromeFacilitatedPaymentsClient::IsWebContentsVisibleOrOccluded() {
  return GetWebContents().GetVisibility() != content::Visibility::HIDDEN;
}

void ChromeFacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {
  facilitated_payments_controller_->Show(
      std::move(bank_account_suggestions),
      std::move(on_payment_account_selected));
}

void ChromeFacilitatedPaymentsClient::ShowPaymentLinkPrompt(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsAppInfoList>
        app_suggestions,
    base::OnceCallback<void(payments::facilitated::SelectedFopData)>
        on_fop_selected) {
  facilitated_payments_controller_->ShowForPaymentLink(
      ewallet_suggestions, std::move(app_suggestions),
      std::move(on_fop_selected));
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

strike_database::StrikeDatabase*
ChromeFacilitatedPaymentsClient::GetStrikeDatabase() {
  content::BrowserContext* context = GetWebContents().GetBrowserContext();

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  return autofill::StrikeDatabaseFactory::GetForProfile(profile);
}

void ChromeFacilitatedPaymentsClient::InitPixAccountLinkingFlow(
    const url::Origin& pix_payment_page_origin) {
  pix_account_linking_manager_->MaybeShowPixAccountLinkingPrompt(
      pix_payment_page_origin);
}

void ChromeFacilitatedPaymentsClient::ShowPixAccountLinkingPrompt(
    base::OnceCallback<void()> on_accepted,
    base::OnceCallback<void()> on_declined) {
  facilitated_payments_controller_->ShowPixAccountLinkingPrompt(
      std::move(on_accepted), std::move(on_declined));
}

bool ChromeFacilitatedPaymentsClient::HasScreenlockOrBiometricSetup() {
  device_reauth::DeviceAuthParams params(
      base::Seconds(60), device_reauth::DeviceAuthSource::kAutofill);
  auto authenticator = ChromeDeviceAuthenticatorFactory::GetForProfile(
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext()),
      GetWebContents().GetTopLevelNativeWindow(), params);
  return authenticator->CanAuthenticateWithBiometricOrScreenLock();
}

void ChromeFacilitatedPaymentsClient::RegisterAllowlists() {
  if (optimization_guide_decider_) {
    if (base::FeatureList::IsEnabled(payments::facilitated::kEwalletPayments)) {
      optimization_guide_decider_->RegisterOptimizationTypes(
          {optimization_guide::proto::EWALLET_MERCHANT_ALLOWLIST});
    }
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::A2A_MERCHANT_ALLOWLIST});
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST});
  }
}

void ChromeFacilitatedPaymentsClient::
    SetFacilitatedPaymentsControllerForTesting(
        std::unique_ptr<FacilitatedPaymentsController> mock_controller) {
  facilitated_payments_controller_ = std::move(mock_controller);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeFacilitatedPaymentsClient);
