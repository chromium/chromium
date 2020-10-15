// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include <vector>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
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
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#endif  // OS_CHROMEOS

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
    content::RenderFrameHost* render_frame_host)
    : shown_dialog_(nullptr),
      frame_routing_id_(content::GlobalFrameRoutingId(
          render_frame_host->GetProcess()->GetID(),
          render_frame_host->GetRoutingID())) {}

ChromePaymentRequestDelegate::~ChromePaymentRequestDelegate() = default;

void ChromePaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  DCHECK_EQ(nullptr, shown_dialog_.get());
  DCHECK_EQ(nullptr, spc_dialog_.get());

  switch (dialog_type_) {
    case DialogType::PAYMENT_REQUEST:
      shown_dialog_ = PaymentRequestDialogView::Create(request, nullptr);
      break;
    case DialogType::SECURE_PAYMENT_CONFIRMATION:
      spc_dialog_ =
          std::make_unique<SecurePaymentConfirmationController>(request);
      shown_dialog_ = spc_dialog_->GetWeakPtr();
      break;
  }

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

  spc_dialog_.reset();
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
      Profile::FromBrowserContext(GetBrowserContextOrNull())
          ->GetOriginalProfile());
}

const std::string& ChromePaymentRequestDelegate::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

bool ChromePaymentRequestDelegate::IsOffTheRecord() const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh)
    return false;
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  return profile && profile->IsOffTheRecord();
}

const GURL& ChromePaymentRequestDelegate::GetLastCommittedURL() const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? content::WebContents::FromRenderFrameHost(rfh)
                   ->GetLastCommittedURL()
             : GURL::EmptyGURL();
}

void ChromePaymentRequestDelegate::DoFullCardRequest(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (rfh && rfh->IsCurrent() && shown_dialog_) {
    shown_dialog_->ShowCvcUnmaskPrompt(
        credit_card, result_delegate,
        content::WebContents::FromRenderFrameHost(rfh));
  }
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
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh)
    return std::string();

  // Check if the profile is authenticated.  Guest profiles or incognito
  // windows may not have a sign in manager, and are considered not
  // authenticated.
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager && identity_manager->HasPrimaryAccount())
    return identity_manager->GetPrimaryAccountInfo().email;

  return std::string();
}

PrefService* ChromePaymentRequestDelegate::GetPrefService() {
  return Profile::FromBrowserContext(GetBrowserContextOrNull())->GetPrefs();
}

bool ChromePaymentRequestDelegate::IsBrowserWindowActive() const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh || !rfh->IsCurrent())
    return false;

  Browser* browser = chrome::FindBrowserWithWebContents(
      content::WebContents::FromRenderFrameHost(rfh));
  return browser && browser->window() && browser->window()->IsActive();
}

std::unique_ptr<autofill::InternalAuthenticator>
ChromePaymentRequestDelegate::CreateInternalAuthenticator() const {
  // This authenticator can be used in a cross-origin iframe only if the
  // top-level frame allowed it with Feature Policy, e.g., with allow="payment"
  // iframe attribute. The secure payment confirmation dialog displays the
  // top-level origin in its UI before the user can click on the [Verify] button
  // to invoke this authenticator.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? std::make_unique<content::InternalAuthenticatorImpl>(
                   rfh->GetMainFrame())
             : nullptr;
}

scoped_refptr<PaymentManifestWebDataService>
ChromePaymentRequestDelegate::GetPaymentManifestWebDataService() const {
  return WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
      Profile::FromBrowserContext(GetBrowserContextOrNull()),
      ServiceAccessType::EXPLICIT_ACCESS);
}

PaymentRequestDisplayManager*
ChromePaymentRequestDelegate::GetDisplayManager() {
  return PaymentRequestDisplayManagerFactory::GetForBrowserContext(
      GetBrowserContextOrNull());
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
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? SslValidityChecker::GetInvalidSslCertificateErrorMessage(
                   content::WebContents::FromRenderFrameHost(rfh))
             : "";
}

bool ChromePaymentRequestDelegate::SkipUiForBasicCard() const {
  return false;  // Only tests do this.
}

std::string ChromePaymentRequestDelegate::GetTwaPackageName() const {
#if defined(OS_CHROMEOS)
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh || !rfh->IsCurrent())
    return "";

  auto* apk_web_app_service = chromeos::ApkWebAppService::Get(
      Profile::FromBrowserContext(rfh->GetBrowserContext()));
  if (!apk_web_app_service)
    return "";

  base::Optional<std::string> twa_package_name =
      apk_web_app_service->GetPackageNameForWebApp(
          content::WebContents::FromRenderFrameHost(rfh)
              ->GetLastCommittedURL());

  return twa_package_name.has_value() ? twa_package_name.value() : "";
#else
  return "";
#endif  // OS_CHROMEOS
}

PaymentRequestDialog* ChromePaymentRequestDelegate::GetDialogForTesting() {
  return shown_dialog_.get();
}

content::BrowserContext* ChromePaymentRequestDelegate::GetBrowserContextOrNull()
    const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh ? rfh->GetBrowserContext() : nullptr;
}

}  // namespace payments
