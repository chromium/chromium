// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

namespace {

const url::Origin& GetAllowedOrigin() {
  static const base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GaiaUrls::GetInstance()->gaia_url()));
  CHECK(!origin->opaque());
  return *origin;
}

bool ShouldExposeMojoApi(content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->IsInPrimaryMainFrame());
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return false;
  }
  // Restrict to allowed origin only.
  return url::Origin::Create(navigation_handle->GetURL()) == GetAllowedOrigin();
}

}  // namespace

class SyncEncryptionKeysTabHelper::EncryptionKeyApi
    : public chrome::mojom::SyncEncryptionKeysExtension {
 public:
  EncryptionKeyApi(content::WebContents* web_contents,
                   syncer::SyncService* sync_service)
      : sync_service_(sync_service), receivers_(web_contents, this) {
    DCHECK(web_contents);
    DCHECK(sync_service);
  }

  EncryptionKeyApi(const EncryptionKeyApi&) = delete;
  EncryptionKeyApi& operator=(const EncryptionKeyApi&) = delete;

  void BindReceiver(mojo::PendingAssociatedReceiver<
                        chrome::mojom::SyncEncryptionKeysExtension> receiver,
                    content::RenderFrameHost* rfh) {
    receivers_.Bind(rfh, std::move(receiver));
  }

  // chrome::mojom::SyncEncryptionKeysExtension:
  void SetEncryptionKeys(
      const std::string& gaia_id,
      const std::vector<std::vector<uint8_t>>& encryption_keys,
      int last_key_version,
      SetEncryptionKeysCallback callback) override {
    // Extra safeguard, e.g. to guard against subframes.
    if (receivers_.GetCurrentTargetFrame()->GetLastCommittedOrigin() !=
        GetAllowedOrigin()) {
      return;
    }

    sync_service_->AddTrustedVaultDecryptionKeysFromWeb(
        gaia_id, encryption_keys, last_key_version);
    std::move(callback).Run();
  }

  void AddTrustedRecoveryMethod(
      const std::string& gaia_id,
      const std::vector<uint8_t>& public_key,
      int method_type_hint,
      AddTrustedRecoveryMethodCallback callback) override {
    if (!base::FeatureList::IsEnabled(
            switches::kSyncTrustedVaultPassphraseRecovery)) {
      return;
    }

    // Extra safeguard, e.g. to guard against subframes.
    if (receivers_.GetCurrentTargetFrame()->GetLastCommittedOrigin() !=
        GetAllowedOrigin()) {
      return;
    }

    sync_service_->AddTrustedVaultRecoveryMethodFromWeb(
        gaia_id, public_key, method_type_hint, std::move(callback));
  }

 private:
  const raw_ptr<syncer::SyncService> sync_service_;

  content::RenderFrameHostReceiverSet<
      chrome::mojom::SyncEncryptionKeysExtension>
      receivers_;
};

// static
void SyncEncryptionKeysTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (FromWebContents(web_contents)) {
    return;
  }

  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }

  syncer::SyncService* sync_service = SyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!sync_service) {
    return;
  }

  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new SyncEncryptionKeysTabHelper(
                                web_contents, sync_service)));
}

// static
void SyncEncryptionKeysTabHelper::BindSyncEncryptionKeysExtension(
    mojo::PendingAssociatedReceiver<chrome::mojom::SyncEncryptionKeysExtension>
        receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab_helper = SyncEncryptionKeysTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->encryption_key_api_->BindReceiver(std::move(receiver), rfh);
}

SyncEncryptionKeysTabHelper::SyncEncryptionKeysTabHelper(
    content::WebContents* web_contents,
    syncer::SyncService* sync_service)
    : content::WebContentsUserData<SyncEncryptionKeysTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      sync_service_(sync_service) {
  DCHECK(sync_service);
}

SyncEncryptionKeysTabHelper::~SyncEncryptionKeysTabHelper() = default;

void SyncEncryptionKeysTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!ShouldExposeMojoApi(navigation_handle)) {
    encryption_key_api_.reset();
  } else if (!encryption_key_api_) {
    encryption_key_api_ =
        std::make_unique<EncryptionKeyApi>(web_contents(), sync_service_);
  }
}

bool SyncEncryptionKeysTabHelper::IsEncryptionKeysApiBoundForTesting() {
  return encryption_key_api_ != nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SyncEncryptionKeysTabHelper);
