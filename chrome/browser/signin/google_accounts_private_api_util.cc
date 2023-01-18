// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/google_accounts_private_api_util.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "google_apis/gaia/gaia_urls.h"

const url::Origin& GetAllowedGoogleAccountsOrigin() {
  const url::Origin& origin = GaiaUrls::GetInstance()->gaia_origin();
  CHECK(!origin.opaque());
  return origin;
}

bool ShouldExposeGoogleAccountsPrivateApi(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return false;
  }

  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
  const url::Origin rfh_origin = rfh->GetLastCommittedOrigin();
  // Restrict to allowed origin and only if site isolation requires a dedicated
  // process. The host is compared explicitly to confirm that the allowed origin
  // uses a dedicated process, rather than sharing process with eTLD+1.
  return rfh_origin == GetAllowedGoogleAccountsOrigin() &&
         rfh->GetSiteInstance()->RequiresDedicatedProcess() &&
         rfh->GetSiteInstance()->GetSiteURL().host() ==
             GetAllowedGoogleAccountsOrigin().host();
}
