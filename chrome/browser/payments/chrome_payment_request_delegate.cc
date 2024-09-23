// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/chrome_payment_request_delegate.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"
#include "components/autofill/core/browser/geo/region_data_loader_impl.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/region_combobox_model.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/content/ssl_validity_checker.h"
#include "components/payments/core/payment_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
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

bool FrameSupportsPayments(content::RenderFrameHost* rfh) {
  return rfh && rfh->IsActive() && rfh->IsRenderFrameLive() &&
         rfh->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kPayment);
}

}  // namespace

ChromePaymentRequestDelegate::ChromePaymentRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : shown_dialog_(nullptr),
      frame_routing_id_(render_frame_host->GetGlobalId()),
      twa_package_helper_(FrameSupportsPayments(render_frame_host)
                              ? render_frame_host
                              : nullptr) {}

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

  // The shown_dialog_ may have been an SPC dialog, in which case we own the
  // object directly and need to clean it up here.
  spc_dialog_.reset();

  // The 'no-credentials' dialog for SPC is currently handled separately from
  // spc_dialog_ (and shown_dialog_), and so needs to separately be closed and
  // cleaned up.
  if (spc_no_creds_dialog_) {
    spc_no_creds_dialog_->CloseDialog();
    spc_no_creds_dialog_.reset();
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
  return autofill::PersonalDataManagerFactory::GetForBrowserContext(
      GetBrowserContextOrNull());
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
  return FrameSupportsPayments(rfh) ? rfh->GetMainFrame()->GetLastCommittedURL()
                                    : GURL::EmptyGURL();
}

autofill::AddressNormalizer*
ChromePaymentRequestDelegate::GetAddressNormalizer() {
  return autofill::AddressNormalizerFactory::GetInstance();
}

autofill::RegionDataLoader*
ChromePaymentRequestDelegate::GetRegionDataLoader() {
  return new autofill::RegionDataLoaderImpl(GetAddressInputSource().release(),
                                            GetAddressInputStorage().release(),
                                            GetApplicationLocale());
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
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
        .email;
  }

  return std::string();
}

PrefService* ChromePaymentRequestDelegate::GetPrefService() {
  return Profile::FromBrowserContext(GetBrowserContextOrNull())->GetPrefs();
}

bool ChromePaymentRequestDelegate::IsBrowserWindowActive() const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!FrameSupportsPayments(rfh))
    return false;

  Browser* browser = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(rfh));
  return browser && browser->window() && browser->window()->IsActive();
}

void ChromePaymentRequestDelegate::ShowNoMatchingPaymentCredentialDialog(
    const std::u16string& merchant_name,
    const std::string& rp_id,
    base::OnceClosure response_callback,
    base::OnceClosure opt_out_callback) {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!FrameSupportsPayments(rfh))
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  spc_no_creds_dialog_ = SecurePaymentConfirmationNoCreds::Create();
  spc_no_creds_dialog_->ShowDialog(web_contents, merchant_name, rp_id,
                                   std::move(response_callback),
                                   std::move(opt_out_callback));
}

content::RenderFrameHost* ChromePaymentRequestDelegate::GetRenderFrameHost()
    const {
  return content::RenderFrameHost::FromID(frame_routing_id_);
}

std::unique_ptr<webauthn::InternalAuthenticator>
ChromePaymentRequestDelegate::CreateInternalAuthenticator() const {
  // This authenticator can be used in a cross-origin iframe only if the
  // top-level frame allowed it with Permissions Policy, e.g., with
  // allow="payment" iframe attribute. The secure payment confirmation dialog
  // displays the top-level origin in its UI before the user can click on the
  // [Verify] button to invoke this authenticator.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  // Lifetime of the created authenticator is externally managed by the
  // authenticator factory, but is generally tied to the RenderFrame by
  // listening for `RenderFrameDeleted()`. `FrameSupportsPayments()` already
  // performs this check on our behalf, so the DCHECK() here is just for
  // documentation purposes: this ensures that `RenderFrameDeleted()` will be
  // called at some point.
  if (!FrameSupportsPayments(rfh))
    return nullptr;
  DCHECK(rfh->IsRenderFrameLive());
  return std::make_unique<content::InternalAuthenticatorImpl>(rfh);
}

scoped_refptr<PaymentManifestWebDataService>
ChromePaymentRequestDelegate::GetPaymentManifestWebDataService() const {
  return webdata_services::WebDataServiceWrapperFactory::
      GetPaymentManifestWebDataServiceForBrowserContext(
          GetBrowserContextOrNull(), ServiceAccessType::EXPLICIT_ACCESS);
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
  return FrameSupportsPayments(rfh)
             ? SslValidityChecker::GetInvalidSslCertificateErrorMessage(
                   content::WebContents::FromRenderFrameHost(rfh))
             : "";
}

void ChromePaymentRequestDelegate::GetTwaPackageName(
    GetTwaPackageNameCallback callback) const {
  twa_package_helper_.GetTwaPackageName(std::move(callback));
}

PaymentRequestDialog* ChromePaymentRequestDelegate::GetDialogForTesting() {
  return shown_dialog_.get();
}

SecurePaymentConfirmationNoCreds*
ChromePaymentRequestDelegate::GetNoMatchingCredentialsDialogForTesting() {
  return spc_no_creds_dialog_.get();
}

std::optional<base::UnguessableToken>
ChromePaymentRequestDelegate::GetChromeOSTWAInstanceId() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!FrameSupportsPayments(rfh)) {
    return std::nullopt;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return std::nullopt;
  }
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  if (!profile) {
    return std::nullopt;
  }
  auto* app_instance_tracker =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppInstanceTracker();
  if (!app_instance_tracker) {
    return std::nullopt;
  }
  const apps::BrowserAppInstance* app_instance =
      app_instance_tracker->GetAppInstance(web_contents);
  if (!app_instance) {
    return std::nullopt;
  }
  return app_instance->id;
#else
  return std::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

const base::WeakPtr<PaymentUIObserver>
ChromePaymentRequestDelegate::GetPaymentUIObserver() const {
  return nullptr;
}

content::BrowserContext* ChromePaymentRequestDelegate::GetBrowserContextOrNull()
    const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh ? rfh->GetBrowserContext() : nullptr;
}

}  // namespace payments
