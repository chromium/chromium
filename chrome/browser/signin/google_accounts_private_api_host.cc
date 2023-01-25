// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/google_accounts_private_api_host.h"

#include "chrome/browser/signin/google_accounts_private_api_util.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

GoogleAccountsPrivateApiHost::GoogleAccountsPrivateApiHost(
    content::RenderFrameHost* rfh,
    base::RepeatingCallback<void(const std::string&)>
        on_consent_result_callback)
    : DocumentUserData<GoogleAccountsPrivateApiHost>(rfh),
      receiver_(this),
      on_consent_result_callback_(std::move(on_consent_result_callback)) {}

GoogleAccountsPrivateApiHost::~GoogleAccountsPrivateApiHost() = default;

DOCUMENT_USER_DATA_KEY_IMPL(GoogleAccountsPrivateApiHost);

void GoogleAccountsPrivateApiHost::BindReceiver(
    mojo::PendingAssociatedReceiver<
        chrome::mojom::GoogleAccountsPrivateApiExtension> receiver) {
  receiver_.Bind(std::move(receiver));
}

void GoogleAccountsPrivateApiHost::SetConsentResult(
    const std::string& consent_result) {
#if !BUILDFLAG(IS_ANDROID)
  if (!render_frame_host().IsInPrimaryMainFrame() ||
      !on_consent_result_callback_) {
    return;
  }

  on_consent_result_callback_.Run(consent_result);
#endif  // !BUILDFLAG(IS_ANDROID)
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
    base::RepeatingCallback<void(const std::string&)>
        on_consent_result_callback,
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (ShouldExposeGoogleAccountsPrivateApi(navigation_handle)) {
    GoogleAccountsPrivateApiHost::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost(),
        std::move(on_consent_result_callback));
  }
}
