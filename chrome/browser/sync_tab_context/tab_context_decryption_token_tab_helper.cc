// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_tab_context/tab_context_decryption_token_tab_helper.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/tab_context_sync_service_factory.h"
#include "chrome/common/tab_context_decryption_token_extension.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

bool ShouldExposeTabContextPrivateApi(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEncryptedTabContextContainer)) {
    return false;
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return false;
  }

  content::RenderFrameHost* const rfh = navigation_handle->GetRenderFrameHost();
  const url::Origin rfh_origin = rfh->GetLastCommittedOrigin();
  const url::Origin allowed_origin =
      sync_tab_context::GetAllowedTabContextOrigin();
  return rfh_origin == allowed_origin &&
         rfh->GetSiteInstance()->RequiresDedicatedProcess() &&
         rfh->GetSiteInstance()->GetSecurityPrincipal().GetHost() ==
             allowed_origin.host();
}

class DecryptionTokenApi
    : public chrome::mojom::TabContextDecryptionTokenExtension,
      public content::DocumentUserData<DecryptionTokenApi> {
 public:
  DecryptionTokenApi(const DecryptionTokenApi&) = delete;
  DecryptionTokenApi& operator=(const DecryptionTokenApi&) = delete;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::TabContextDecryptionTokenExtension> receiver,
      content::RenderFrameHost* rfh) {
    receivers_.Bind(rfh, std::move(receiver));
  }

  // chrome::mojom::TabContextDecryptionTokenExtension:
  void GetContainerDecryptionToken(
      const std::string& obfuscated_gaia_id,
      const base::Uuid& container_id,
      GetContainerDecryptionTokenCallback callback) override {
    CHECK(base::FeatureList::IsEnabled(
        syncer::kSyncEncryptedTabContextContainer));

    // Although binding is restricted to allowed origins during navigation,
    // a compromised renderer attempting to send an unauthorized IPC message
    // must be terminated via ReportBadMessage rather than a browser CHECK.
    if (receivers_.CurrentTargetFrame().GetLastCommittedOrigin() !=
        sync_tab_context::GetAllowedTabContextOrigin()) {
      mojo::ReportBadMessage("TabContextDecryptionToken: Unauthorized origin");
      return;
    }

    Profile* const profile = Profile::FromBrowserContext(
        receivers_.CurrentTargetFrame().GetBrowserContext());
    if (profile->IsOffTheRecord()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!identity_manager) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    const CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (primary_account.gaia != GaiaId(obfuscated_gaia_id)) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    sync_tab_context::TabContextSyncService* const service =
        TabContextSyncServiceFactory::GetForProfile(profile);
    if (!service) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    if (!container_id.is_valid()) {
      mojo::ReportBadMessage("TabContextDecryptionToken: Invalid container ID");
      return;
    }

    service->GetContainerAccessToken(
        sync_tab_context::ContainerId(container_id),
        base::BindOnce(
            [](GetContainerDecryptionTokenCallback callback,
               std::optional<std::string> token) {
              if (!token) {
                std::move(callback).Run(std::nullopt);
                return;
              }
              std::vector<uint8_t> bytes(token->begin(), token->end());
              std::move(callback).Run(std::move(bytes));
            },
            std::move(callback)));
  }

 private:
  explicit DecryptionTokenApi(content::RenderFrameHost* rfh)
      : DocumentUserData<DecryptionTokenApi>(rfh),
        receivers_(content::WebContents::FromRenderFrameHost(rfh), this) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  content::RenderFrameHostReceiverSet<
      chrome::mojom::TabContextDecryptionTokenExtension>
      receivers_;
};

DOCUMENT_USER_DATA_KEY_IMPL(DecryptionTokenApi);

}  // namespace

// static
void TabContextDecryptionTokenTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents);

  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEncryptedTabContextContainer)) {
    return;
  }

  if (FromWebContents(web_contents)) {
    return;
  }

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new TabContextDecryptionTokenTabHelper(web_contents)));
}

// static
void TabContextDecryptionTokenTabHelper::BindTabContextDecryptionTokenExtension(
    mojo::PendingAssociatedReceiver<
        chrome::mojom::TabContextDecryptionTokenExtension> receiver,
    content::RenderFrameHost* rfh) {
  DecryptionTokenApi* const api =
      DecryptionTokenApi::GetForCurrentDocument(rfh);
  if (!api) {
    return;
  }
  api->BindReceiver(std::move(receiver), rfh);
}

TabContextDecryptionTokenTabHelper::TabContextDecryptionTokenTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<TabContextDecryptionTokenTabHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

TabContextDecryptionTokenTabHelper::~TabContextDecryptionTokenTabHelper() =
    default;

void TabContextDecryptionTokenTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (ShouldExposeTabContextPrivateApi(navigation_handle)) {
    DecryptionTokenApi::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabContextDecryptionTokenTabHelper);
