// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
class NavigationHandle;
}  // namespace content

namespace syncer {
class SyncService;
}  // namespace syncer

// TrustedVaultEncryptionKeysTabHelper is responsible for installing Mojo API in
// order to receive client encryption keys for //components/trusted_vault from
// the renderer process.
class TrustedVaultEncryptionKeysTabHelper
    : public content::WebContentsUserData<TrustedVaultEncryptionKeysTabHelper>,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  static void BindTrustedVaultEncryptionKeysExtension(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::TrustedVaultEncryptionKeysExtension> receiver,
      content::RenderFrameHost* rfh);

  TrustedVaultEncryptionKeysTabHelper(
      const TrustedVaultEncryptionKeysTabHelper&) = delete;
  TrustedVaultEncryptionKeysTabHelper& operator=(
      const TrustedVaultEncryptionKeysTabHelper&) = delete;

  ~TrustedVaultEncryptionKeysTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // TODO(https://crbug.com/1281874): Update this to check if the Mojo interface
  // is bound.
  bool HasEncryptionKeysApiForTesting(
      content::RenderFrameHost* render_frame_host);

 private:
  friend class content::WebContentsUserData<
      TrustedVaultEncryptionKeysTabHelper>;

  // Null `sync_service` is interpreted as incognito (when it comes to metrics).
  TrustedVaultEncryptionKeysTabHelper(content::WebContents* web_contents,
                                      syncer::SyncService* sync_service);

  // Null `sync_service_` is interpreted as incognito (when it comes to
  // metrics).
  const raw_ptr<syncer::SyncService> sync_service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SYNC_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_
