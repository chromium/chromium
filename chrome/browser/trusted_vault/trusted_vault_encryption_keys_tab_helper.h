// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_
#define CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class EnclaveManager;

namespace content {
class RenderFrameHost;
class WebContents;
class NavigationHandle;
}  // namespace content

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

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

  // TODO(crbug.com/40812482): Update this to check if the Mojo interface
  // is bound.
  bool HasEncryptionKeysApiForTesting(
      content::RenderFrameHost* render_frame_host);

 private:
  friend class content::WebContentsUserData<
      TrustedVaultEncryptionKeysTabHelper>;

  // Null `trusted_vault_service_` is interpreted as incognito (when it comes to
  // metrics). Null `enclave_manager_` means that no passkeys enclave service is
  // active.
  TrustedVaultEncryptionKeysTabHelper(
      content::WebContents* web_contents,
      trusted_vault::TrustedVaultService* trusted_vault_service,
      EnclaveManager* enclave_manager);

  // Null `trusted_vault_service_` is interpreted as incognito (when it comes to
  // metrics).
  const raw_ptr<trusted_vault::TrustedVaultService> trusted_vault_service_;
  const raw_ptr<EnclaveManager> enclave_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TRUSTED_VAULT_TRUSTED_VAULT_ENCRYPTION_KEYS_TAB_HELPER_H_
