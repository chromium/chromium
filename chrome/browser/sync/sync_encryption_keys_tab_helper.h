// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
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

// SyncEncryptionKeysTabHelper is responsible for installing Mojo API in order
// to receive sync encryption keys from the renderer process.
class SyncEncryptionKeysTabHelper
    : public content::WebContentsUserData<SyncEncryptionKeysTabHelper>,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  static void BindSyncEncryptionKeysExtension(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::SyncEncryptionKeysExtension> receiver,
      content::RenderFrameHost* rfh);

  SyncEncryptionKeysTabHelper(const SyncEncryptionKeysTabHelper&) = delete;
  SyncEncryptionKeysTabHelper& operator=(const SyncEncryptionKeysTabHelper&) =
      delete;

  ~SyncEncryptionKeysTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // TODO(https://crbug.com/1281874): Update this to check if the Mojo interface
  // is bound.
  bool HasEncryptionKeysApiForTesting(
      content::RenderFrameHost* render_frame_host);

 private:
  friend class content::WebContentsUserData<SyncEncryptionKeysTabHelper>;

  SyncEncryptionKeysTabHelper(content::WebContents* web_contents,
                              syncer::SyncService* sync_service);

  const raw_ptr<syncer::SyncService> sync_service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_
