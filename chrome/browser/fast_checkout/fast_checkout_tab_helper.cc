// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_tab_helper.h"

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
bool IsCartOrCheckoutUrl(const GURL& url) {
  return commerce_heuristics::IsVisitCheckout(url) ||
         commerce_heuristics::IsVisitCart(url);
}
}  // namespace

FastCheckoutTabHelper::FastCheckoutTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FastCheckoutTabHelper>(*web_contents) {}

FastCheckoutTabHelper::~FastCheckoutTabHelper() = default;

void FastCheckoutTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about top-level navigations.
  if (!navigation_handle || !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Shopping sites should be http or https - save heuristics if this URL
  // does not satisfy that.
  const GURL& url = navigation_handle->GetURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  FetchCapabilities(url);
  if (autofill::ContentAutofillClient* client =
          autofill::ContentAutofillClient::FromWebContents(web_contents())) {
    if (auto* fast_checkout_client = client->GetFastCheckoutClient()) {
      fast_checkout_client->OnNavigation(url, IsCartOrCheckoutUrl(url));
    }
  }
}

void FastCheckoutTabHelper::FetchCapabilities(const GURL& url) {
  // Check for both checkout and cart URLs because some websites use cart URLs
  // throughout their whole checkout funnel.
  if (IsCartOrCheckoutUrl(url)) {
    PrefService* pref_service =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetPrefs();
    if (!pref_service) {
      return;
    }
    if (!autofill::prefs::IsAutofillProfileEnabled(pref_service) ||
        !autofill::prefs::IsAutofillPaymentMethodsEnabled(pref_service)) {
      return;
    }

    FastCheckoutCapabilitiesFetcher* fetcher =
        FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
            GetWebContents().GetBrowserContext());
    if (!fetcher) {
      return;
    }

    fetcher->FetchCapabilities();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutTabHelper);
