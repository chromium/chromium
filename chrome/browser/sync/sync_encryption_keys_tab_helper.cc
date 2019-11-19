// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_binding_set.h"
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
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
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
      : sync_service_(sync_service), bindings_(web_contents, this) {
    DCHECK(web_contents);
    DCHECK(sync_service);
  }

  // chrome::mojom::SyncEncryptionKeysExtension:
  void SetEncryptionKeys(const std::vector<std::string>& encryption_keys,
                         const std::string& gaia_id,
                         SetEncryptionKeysCallback callback) override {
    CHECK_EQ(bindings_.GetCurrentTargetFrame()->GetLastCommittedOrigin(),
             GetAllowedOrigin());

    sync_service_->GetUserSettings()->AddTrustedVaultDecryptionKeys(
        gaia_id, encryption_keys);
    std::move(callback).Run();
  }

 private:
  syncer::SyncService* const sync_service_;

  content::WebContentsFrameBindingSet<
      chrome::mojom::SyncEncryptionKeysExtension>
      bindings_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionKeyApi);
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

  syncer::SyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!sync_service) {
    return;
  }

  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new SyncEncryptionKeysTabHelper(
                                web_contents, sync_service)));
}

SyncEncryptionKeysTabHelper::SyncEncryptionKeysTabHelper(
    content::WebContents* web_contents,
    syncer::SyncService* sync_service)
    : content::WebContentsObserver(web_contents), sync_service_(sync_service) {
  DCHECK(web_contents);
  DCHECK(sync_service);
}

SyncEncryptionKeysTabHelper::~SyncEncryptionKeysTabHelper() = default;

void SyncEncryptionKeysTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (!ShouldExposeMojoApi(navigation_handle)) {
    encryption_key_api_.reset();
  } else if (!encryption_key_api_) {
    encryption_key_api_ =
        std::make_unique<EncryptionKeyApi>(web_contents(), sync_service_);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SyncEncryptionKeysTabHelper)
