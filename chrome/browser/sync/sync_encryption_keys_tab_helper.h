// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
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

  ~SyncEncryptionKeysTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<SyncEncryptionKeysTabHelper>;

  SyncEncryptionKeysTabHelper(content::WebContents* web_contents,
                              syncer::SyncService* sync_service);

  syncer::SyncService* const sync_service_;

  // EncryptionKeyApi represent the actual exposure of the Mojo API (i.e.
  // chrome::mojom::SyncEncryptionKeysExtension) to the renderer. Instantiated
  // only for allowed origins.
  class EncryptionKeyApi;
  std::unique_ptr<EncryptionKeyApi> encryption_key_api_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SyncEncryptionKeysTabHelper);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ENCRYPTION_KEYS_TAB_HELPER_H_
