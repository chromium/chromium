// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"
#include "components/autofill/core/browser/geo/region_data_loader_impl.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/region_combobox_model.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/core/payment_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace payments {

namespace {

std::unique_ptr<::i18n::addressinput::Source> GetAddressInputSource() {
  return std::unique_ptr<::i18n::addressinput::Source>(
      new autofill::ChromeMetadataSource(
          I18N_ADDRESS_VALIDATION_DATA_URL,
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory()));
}

std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage() {
  return autofill::ValidationRulesStorageFactory::CreateStorage();
}

}  // namespace

ChromePaymentRequestDelegate::ChromePaymentRequestDelegate(
    content::WebContents* web_contents)
    : shown_dialog_(nullptr), web_contents_(web_contents) {}

ChromePaymentRequestDelegate::~ChromePaymentRequestDelegate() {}

void ChromePaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  DCHECK_EQ(nullptr, shown_dialog_);
  shown_dialog_ = new payments::PaymentRequestDialogView(request, nullptr);
  shown_dialog_->ShowDialog();
}

void ChromePaymentRequestDelegate::RetryDialog() {
  if (shown_dialog_)
    shown_dialog_->RetryDialog();
}

void ChromePaymentRequestDelegate::CloseDialog() {
  if (shown_dialog_) {
    shown_dialog_->CloseDialog();
    shown_dialog_ = nullptr;
  }
}

void ChromePaymentRequestDelegate::ShowErrorMessage() {
  if (shown_dialog_)
    shown_dialog_->ShowErrorMessage();
}

void ChromePaymentRequestDelegate::ShowProcessingSpinner() {
  if (shown_dialog_)
    shown_dialog_->ShowProcessingSpinner();
}

autofill::PersonalDataManager*
ChromePaymentRequestDelegate::GetPersonalDataManager() {
  // Autofill uses the original profile's PersonalDataManager to make data
  // available in incognito, so PaymentRequest should do the same.
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetOriginalProfile());
}

const std::string& ChromePaymentRequestDelegate::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

bool ChromePaymentRequestDelegate::IsIncognito() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile && profile->IsIncognitoProfile();
}

const GURL& ChromePaymentRequestDelegate::GetLastCommittedURL() const {
  return web_contents_->GetLastCommittedURL();
}

void ChromePaymentRequestDelegate::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  if (shown_dialog_)
    shown_dialog_->ShowCvcUnmaskPrompt(credit_card, result_delegate,
                                       web_contents_);
}

autofill::RegionDataLoader*
ChromePaymentRequestDelegate::GetRegionDataLoader() {
  return new autofill::RegionDataLoaderImpl(GetAddressInputSource().release(),
                                            GetAddressInputStorage().release(),
                                            GetApplicationLocale());
}

autofill::AddressNormalizer*
ChromePaymentRequestDelegate::GetAddressNormalizer() {
  return autofill::AddressNormalizerFactory::GetInstance();
}

ukm::UkmRecorder* ChromePaymentRequestDelegate::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

std::string ChromePaymentRequestDelegate::GetAuthenticatedEmail() const {
  // Check if the profile is authenticated.  Guest profiles or incognito
  // windows may not have a sign in manager, and are considered not
  // authenticated.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager && identity_manager->HasPrimaryAccount())
    return identity_manager->GetPrimaryAccountInfo().email;

  return std::string();
}

PrefService* ChromePaymentRequestDelegate::GetPrefService() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}

bool ChromePaymentRequestDelegate::IsBrowserWindowActive() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser && browser->window() && browser->window()->IsActive();
}

scoped_refptr<PaymentManifestWebDataService>
ChromePaymentRequestDelegate::GetPaymentManifestWebDataService() const {
  return WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      ServiceAccessType::EXPLICIT_ACCESS);
}

PaymentRequestDisplayManager*
ChromePaymentRequestDelegate::GetDisplayManager() {
  return PaymentRequestDisplayManagerFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

void ChromePaymentRequestDelegate::EmbedPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  if (shown_dialog_) {
    shown_dialog_->ShowPaymentHandlerScreen(url, std::move(callback));
  } else {
    std::move(callback).Run(/*success=*/false,
                            /*render_process_id=*/0,
                            /*render_frame_id=*/0);
  }
}

bool ChromePaymentRequestDelegate::IsInteractive() const {
  return shown_dialog_ && shown_dialog_->IsInteractive();
}

std::string
ChromePaymentRequestDelegate::GetInvalidSslCertificateErrorMessage() {
  return SslValidityChecker::GetInvalidSslCertificateErrorMessage(
      web_contents_);
}

bool ChromePaymentRequestDelegate::SkipUiForBasicCard() const {
  return false;  // Only tests do this.
}

}  // namespace payments
