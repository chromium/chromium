// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_tab_helper.h"

#include "base/callback_helpers.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

FastCheckoutTabHelper::FastCheckoutTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FastCheckoutTabHelper>(*web_contents) {}

FastCheckoutTabHelper::~FastCheckoutTabHelper() = default;

void FastCheckoutTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about top-level navigations.
  if (!navigation_handle || !navigation_handle->IsInPrimaryMainFrame())
    return;

  // Shopping sites should be http or https - save heuristics if this URL
  // does not satisfy that.
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  if (commerce_heuristics::IsVisitCheckout(navigation_handle->GetURL())) {
    FastCheckoutCapabilitiesFetcher* fetcher =
        FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
            GetWebContents().GetBrowserContext());
    if (!fetcher)
      return;

    // Converting to an origin is fine here. The scheme is known to be
    // http/https and there is no risk associated with origin opaqueness.
    fetcher->FetchAvailability(url::Origin::Create(navigation_handle->GetURL()),
                               base::DoNothing());
    return;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutTabHelper);
