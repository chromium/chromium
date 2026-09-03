// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TAB_CONTEXT_TAB_CONTEXT_DECRYPTION_TOKEN_TAB_HELPER_H_
#define CHROME_BROWSER_SYNC_TAB_CONTEXT_TAB_CONTEXT_DECRYPTION_TOKEN_TAB_HELPER_H_

#include "chrome/common/tab_context_decryption_token_extension.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
class NavigationHandle;
}  // namespace content

// TabContextDecryptionTokenTabHelper installs the Mojo API for allowed
// Google Accounts origins to request container decryption tokens from
// TabContextSyncService.
class TabContextDecryptionTokenTabHelper
    : public content::WebContentsUserData<TabContextDecryptionTokenTabHelper>,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  static void BindTabContextDecryptionTokenExtension(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::TabContextDecryptionTokenExtension> receiver,
      content::RenderFrameHost* rfh);

  TabContextDecryptionTokenTabHelper(
      const TabContextDecryptionTokenTabHelper&) = delete;
  TabContextDecryptionTokenTabHelper& operator=(
      const TabContextDecryptionTokenTabHelper&) = delete;

  ~TabContextDecryptionTokenTabHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<TabContextDecryptionTokenTabHelper>;

  explicit TabContextDecryptionTokenTabHelper(
      content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SYNC_TAB_CONTEXT_TAB_CONTEXT_DECRYPTION_TOKEN_TAB_HELPER_H_
