// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/google_accounts_private_api_host.h"

#include "chrome/browser/signin/google_accounts_private_api_util.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"

GoogleAccountsPrivateApiHost::GoogleAccountsPrivateApiHost(
    content::RenderFrameHost* rfh)
    : DocumentUserData<GoogleAccountsPrivateApiHost>(rfh), receiver_(this) {}

GoogleAccountsPrivateApiHost::~GoogleAccountsPrivateApiHost() = default;

DOCUMENT_USER_DATA_KEY_IMPL(GoogleAccountsPrivateApiHost);

void GoogleAccountsPrivateApiHost::BindReceiver(
    mojo::PendingAssociatedReceiver<
        chrome::mojom::GoogleAccountsPrivateApiExtension> receiver) {
  receiver_.Bind(std::move(receiver));
}

void GoogleAccountsPrivateApiHost::SetConsentResult(
    const std::string& consent_result) {
  // To be implemented.
  // TODO: Add a check for non-primary pages.
}

// static
void GoogleAccountsPrivateApiHost::BindHost(
    mojo::PendingAssociatedReceiver<
        chrome::mojom::GoogleAccountsPrivateApiExtension> receiver,
    content::RenderFrameHost* render_frame_host) {
  GoogleAccountsPrivateApiHost* api_host =
      GoogleAccountsPrivateApiHost::GetForCurrentDocument(render_frame_host);
  if (!api_host) {
    return;
  }

  api_host->BindReceiver(std::move(receiver));
}

// static
void GoogleAccountsPrivateApiHost::CreateReceiver(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (ShouldExposeGoogleAccountsPrivateApi(navigation_handle)) {
    GoogleAccountsPrivateApiHost::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost());
  }
}
