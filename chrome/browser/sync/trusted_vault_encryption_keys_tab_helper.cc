// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/trusted_vault_encryption_keys_tab_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/google_accounts_private_api_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents.h"
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
      AddKeysToTrustedVault(gaia_id, vault_name, keys);
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
        sync_service_ == nullptr);

    // Handle incognito separately (where `sync_service_` is null).
    if (!sync_service_) {
      std::move(callback).Run();
      return;
    }

    sync_service_->AddTrustedVaultRecoveryMethodFromWeb(
        gaia_id, public_key, method_type_hint, std::move(callback));
  }

 private:
  // Null `sync_service` is interpreted as incognito (when it comes to metrics).
  EncryptionKeyApi(content::RenderFrameHost* rfh,
                   syncer::SyncService* sync_service)
      : DocumentUserData<EncryptionKeyApi>(rfh),
        sync_service_(sync_service),
        receivers_(content::WebContents::FromRenderFrameHost(rfh), this) {}

#if !BUILDFLAG(IS_ANDROID)
  void AddKeysToTrustedVault(
      const std::string& gaia_id,
      const std::string& vault_name,
      const std::vector<chrome::mojom::TrustedVaultKeyPtr>& keys) {
    if (keys.empty()) {
      // The renderer enforces this.
      return;
    }
    const absl::optional<trusted_vault::SecurityDomainId> security_domain =
        trusted_vault::GetSecurityDomainByName(vault_name);
    if (!security_domain) {
      // TODO(https://crbug.com/1223853): Add a UMA metric for this case.
      DLOG(ERROR) << "Unknown vault type " << vault_name;
      return;
    }
    switch (*security_domain) {
      case trusted_vault::SecurityDomainId::kChromeSync: {
        base::UmaHistogramBoolean(
            "Sync.TrustedVaultJavascriptSetEncryptionKeysIsIncognito",
            sync_service_ == nullptr);

        // Guard against incognito (where `sync_service_` is null).
        if (!sync_service_) {
          return;
        }
        std::vector<std::vector<uint8_t>> keys_as_bytes(keys.size());
        std::transform(keys.begin(), keys.end(), keys_as_bytes.begin(),
                       [](const chrome::mojom::TrustedVaultKeyPtr& key) {
                         return key->bytes;
                       });
        DCHECK(!keys.empty());  // Checked above.
        const int32_t version = keys.back()->version;
        sync_service_->AddTrustedVaultDecryptionKeysFromWeb(
            gaia_id, keys_as_bytes, version);
      }
    }
  }
#endif

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Null `sync_service_` is interpreted as incognito (when it comes to
  // metrics).
  const raw_ptr<syncer::SyncService> sync_service_;

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

  syncer::SyncService* sync_service = nullptr;

  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    sync_service = SyncServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    if (!sync_service) {
      // Other than incognito, there are a few advanced cases (e.g.
      // command-line flags) that can lead to a null SyncService. In these
      // cases, avoid instantiating the tab helper altogether to avoid polluting
      // metrics.
      return;
    }
  }

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new TrustedVaultEncryptionKeysTabHelper(
                         web_contents, sync_service)));
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
    syncer::SyncService* sync_service)
    : content::WebContentsUserData<TrustedVaultEncryptionKeysTabHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      sync_service_(sync_service) {}

TrustedVaultEncryptionKeysTabHelper::~TrustedVaultEncryptionKeysTabHelper() =
    default;

void TrustedVaultEncryptionKeysTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (ShouldExposeGoogleAccountsPrivateApi(navigation_handle)) {
    EncryptionKeyApi::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost(), sync_service_);
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
