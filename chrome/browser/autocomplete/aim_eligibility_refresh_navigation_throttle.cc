// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_refresh_navigation_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

AimEligibilityRefreshNavigationThrottle::
    AimEligibilityRefreshNavigationThrottle(
        content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

AimEligibilityRefreshNavigationThrottle::
    ~AimEligibilityRefreshNavigationThrottle() = default;

const char* AimEligibilityRefreshNavigationThrottle::GetNameForLogging() {
  return "AimEligibilityRefreshNavigationThrottle";
}

ThrottleCheckResult
AimEligibilityRefreshNavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

ThrottleCheckResult
AimEligibilityRefreshNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

ThrottleCheckResult
AimEligibilityRefreshNavigationThrottle::ProcessNavigation() {
  // Check if the URL is valid and http(s).
  const GURL& url = navigation_handle()->GetURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return PROCEED;
  }

  auto* web_contents = navigation_handle()->GetWebContents();
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* aim_service = AimEligibilityServiceFactory::GetForProfile(profile);
  // Check if the profile is eligible for AIM (e.g., Google is DSE).
  if (!aim_service || !aim_service->IsAimEligible()) {
    return PROCEED;
  }

  // Check if this is a search results URL.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return PROCEED;
  }

  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_provider ||
      !default_search_provider->IsSearchURL(
          url, template_url_service->search_terms_data())) {
    return PROCEED;
  }

  // Check if this search URL has AIM-specific parameters.
  if (!aim_service->HasAimUrlParams(url)) {
    return PROCEED;
  }

  // Refresh eligibility for top-level AIM URL navigations.
  aim_service->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);

  // Do not cancel the navigation.
  return PROCEED;
}

// static
void AimEligibilityRefreshNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // Only add throttle if the feature is enabled.
  if (!base::FeatureList::IsEnabled(omnibox::kAimUrlNavigationFetchEnabled)) {
    return;
  }

  // Ignore navigations that aren't in the primary main frame.
  if (!registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    return;
  }

  // Ignore embedded page navigations.
  content::WebContents* web_contents =
      registry.GetNavigationHandle().GetWebContents();
  if (web_contents != web_contents->GetResponsibleWebContents()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<AimEligibilityRefreshNavigationThrottle>(registry));
}
