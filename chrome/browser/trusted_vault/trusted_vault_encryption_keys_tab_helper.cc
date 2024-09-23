// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/google_accounts_private_api_util.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

namespace {

// EncryptionKeyApi represents the actual exposure of the Mojo API (i.e.
// chrome::mojom::TrustedVaultEncryptionKeysExtension) to the renderer.
// Instantiated only for allowed origins.
class EncryptionKeyApi
    : public chrome::mojom::TrustedVaultEncryptionKeysExtension,
      public content::DocumentUserData<EncryptionKeyApi> {
 public:
  EncryptionKeyApi(const EncryptionKeyApi&) = delete;
  EncryptionKeyApi& operator=(const EncryptionKeyApi&) = delete;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::TrustedVaultEncryptionKeysExtension> receiver,
      content::RenderFrameHost* rfh) {
    receivers_.Bind(rfh, std::move(receiver));
  }

  // chrome::mojom::TrustedVaultEncryptionKeysExtension:
#if !BUILDFLAG(IS_ANDROID)
  void SetEncryptionKeys(
      const std::string& gaia_id,
      base::flat_map<std::string,
                     std::vector<chrome::mojom::TrustedVaultKeyPtr>>
          trusted_vault_keys,
      SetEncryptionKeysCallback callback) override {
    // Extra safeguard.
    if (receivers_.GetCurrentTargetFrame()->GetLastCommittedOrigin() !=
        GetAllowedGoogleAccountsOrigin()) {
      return;
    }

    for (const auto& [vault_name, keys] : trusted_vault_keys) {
      if (keys.empty()) {
        // Checked by the renderer.
        receivers_.ReportBadMessage("empty keys for " + vault_name);
        return;
      }
      const std::optional<trusted_vault::SecurityDomainId> security_domain =
          trusted_vault::GetSecurityDomainByName(vault_name);
      trusted_vault::RecordTrustedVaultSetEncryptionKeysForSecurityDomain(
          security_domain, is_off_the_record_for_uma_
                               ? trusted_vault::IsOffTheRecord::kYes
                               : trusted_vault::IsOffTheRecord::kNo);
      if (!security_domain) {
        DLOG(ERROR) << "Unknown vault type " << vault_name;
        continue;
      }
      AddEncryptionKeysForSecurityDomain(gaia_id, *security_domain, keys);
    }

    std::move(callback).Run();
  }
#endif

  void AddTrustedRecoveryMethod(
      const std::string& gaia_id,
      const std::vector<uint8_t>& public_key,
      int method_type_hint,
      AddTrustedRecoveryMethodCallback callback) override {
    // Extra safeguard.
    if (receivers_.GetCurrentTargetFrame()->GetLastCommittedOrigin() !=
        GetAllowedGoogleAccountsOrigin()) {
      return;
    }

    base::UmaHistogramBoolean(
        "Sync.TrustedVaultJavascriptAddRecoveryMethodIsIncognito",
        trusted_vault_service_ == nullptr);

    // Handle incognito separately (where `trusted_vault_service_` is null).
    if (!trusted_vault_service_) {
      std::move(callback).Run();
      return;
    }

    trusted_vault_service_
        ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync)
        ->AddTrustedRecoveryMethod(gaia_id, public_key, method_type_hint,
                                   std::move(callback));
  }

 private:
  // Null `trusted_vault_service` is interpreted as incognito (when it comes to
  // metrics).
  EncryptionKeyApi(content::RenderFrameHost* rfh,
                   EnclaveManager* enclave_manager,
                   trusted_vault::TrustedVaultService* trusted_vault_service)
      : DocumentUserData<EncryptionKeyApi>(rfh),
        is_off_the_record_for_uma_(
            content::WebContents::FromRenderFrameHost(rfh)
                ->GetBrowserContext()
                ->IsOffTheRecord()),
        trusted_vault_service_(trusted_vault_service),
        enclave_manager_(enclave_manager),
        receivers_(content::WebContents::FromRenderFrameHost(rfh), this) {}

#if !BUILDFLAG(IS_ANDROID)
  void AddEncryptionKeysForSecurityDomain(
      const std::string& gaia_id,
      trusted_vault::SecurityDomainId security_domain,
      const std::vector<chrome::mojom::TrustedVaultKeyPtr>& keys) {
    CHECK(!keys.empty());

    base::UmaHistogramBoolean(
        "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
        trusted_vault_service_ == nullptr);

    // Guard against incognito (where `trusted_vault_service_` is null).
    if (!trusted_vault_service_) {
      return;
    }

    std::vector<std::vector<uint8_t>> keys_as_bytes;
    keys_as_bytes.reserve(keys.size());
    base::ranges::transform(keys, std::back_inserter(keys_as_bytes),
                            &chrome::mojom::TrustedVaultKey::bytes);
    const int32_t last_key_version = keys.back()->version;

    if (security_domain == trusted_vault::SecurityDomainId::kPasskeys) {
      if (enclave_manager_) {
        enclave_manager_->StoreKeys(gaia_id, std::move(keys_as_bytes),
                                    last_key_version);
      }
      return;
    }

    trusted_vault::TrustedVaultClient* trusted_vault_client =
        trusted_vault_service_->GetTrustedVaultClient(security_domain);
    if (!trusted_vault_client) {
      DLOG(ERROR) << "No TrustedVaultClient for security domain "
                  << static_cast<int>(security_domain);
      return;
    }
    trusted_vault_client->StoreKeys(gaia_id, keys_as_bytes, last_key_version);
  }
#endif

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  const bool is_off_the_record_for_uma_;

  const raw_ptr<trusted_vault::TrustedVaultService> trusted_vault_service_;
  const raw_ptr<EnclaveManager> enclave_manager_;

  content::RenderFrameHostReceiverSet<
      chrome::mojom::TrustedVaultEncryptionKeysExtension>
      receivers_;
};

DOCUMENT_USER_DATA_KEY_IMPL(EncryptionKeyApi);

}  // namespace

// static
void TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (FromWebContents(web_contents)) {
    return;
  }

  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  trusted_vault::TrustedVaultService* trusted_vault_service = nullptr;

  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    trusted_vault_service = TrustedVaultServiceFactory::GetForProfile(profile);
    if (!trusted_vault_service) {
      // TODO(crbug.com/40264840): Is it possible? Ideally, this should be
      // replaced with CHECK(trusted_vault_service).
      return;
    }
  }

  EnclaveManager* enclave_manager = nullptr;
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator)) {
    enclave_manager =
        EnclaveManagerFactory::GetAsEnclaveManagerForProfile(profile);
  }
#endif

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new TrustedVaultEncryptionKeysTabHelper(
          web_contents, trusted_vault_service, enclave_manager)));
}

// static
void TrustedVaultEncryptionKeysTabHelper::
    BindTrustedVaultEncryptionKeysExtension(
        mojo::PendingAssociatedReceiver<
            chrome::mojom::TrustedVaultEncryptionKeysExtension> receiver,
        content::RenderFrameHost* rfh) {
  EncryptionKeyApi* encryption_key_api =
      EncryptionKeyApi::GetForCurrentDocument(rfh);
  if (!encryption_key_api) {
    return;
  }
  encryption_key_api->BindReceiver(std::move(receiver), rfh);
}

TrustedVaultEncryptionKeysTabHelper::TrustedVaultEncryptionKeysTabHelper(
    content::WebContents* web_contents,
    trusted_vault::TrustedVaultService* trusted_vault_service,
    EnclaveManager* enclave_manager)
    : content::WebContentsUserData<TrustedVaultEncryptionKeysTabHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      trusted_vault_service_(trusted_vault_service),
      enclave_manager_(enclave_manager) {}

TrustedVaultEncryptionKeysTabHelper::~TrustedVaultEncryptionKeysTabHelper() =
    default;

void TrustedVaultEncryptionKeysTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (ShouldExposeGoogleAccountsPrivateApi(navigation_handle)) {
    EncryptionKeyApi::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost(), enclave_manager_,
        trusted_vault_service_);
  } else {
    // NavigationHandle::GetRenderFrameHost() can only be accessed after a
    // response has been delivered for processing, or after the navigation fails
    // with an error page. See NavigationHandle::GetRenderFrameHost() for the
    // details.
    if (navigation_handle->HasCommitted() &&
        navigation_handle->GetRenderFrameHost()) {
      // The document this navigation is committing from should not have
      // the existing EncryptionKeyApi.
      CHECK(!EncryptionKeyApi::GetForCurrentDocument(
          navigation_handle->GetRenderFrameHost()));
    }
  }
}

bool TrustedVaultEncryptionKeysTabHelper::HasEncryptionKeysApiForTesting(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  return EncryptionKeyApi::GetForCurrentDocument(render_frame_host);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TrustedVaultEncryptionKeysTabHelper);
