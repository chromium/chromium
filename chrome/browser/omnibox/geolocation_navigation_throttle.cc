// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/geolocation_navigation_throttle.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
const char kXGeoHeaderName[] = "X-Geo";
}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
GeolocationNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    return nullptr;
  }
  // We only add the X-Geo header to main frame navigations.
  if (!registry.GetNavigationHandle().IsInOutermostMainFrame()) {
    return nullptr;
  }

  return std::make_unique<GeolocationNavigationThrottle>(registry);
}

GeolocationNavigationThrottle::GeolocationNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GeolocationNavigationThrottle::~GeolocationNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

const char* GeolocationNavigationThrottle::GetNameForLogging() {
  return "GeolocationNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::ProcessNavigation() {
  if (!(navigation_handle()->GetPageTransition() &
        ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    return PROCEED;
  }

  content::BrowserContext* context =
      navigation_handle()->GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return PROCEED;
  }

  GeolocationHeaderService* service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  if (!service) {
    return PROCEED;
  }

  std::optional<std::string> geo_header =
      service->GetLocationHeader(navigation_handle()->GetURL());
  if (geo_header) {
    navigation_handle()->SetRequestHeader(kXGeoHeaderName, *geo_header);
  } else if (navigation_handle()->WasServerRedirect()) {
    navigation_handle()->RemoveRequestHeader(kXGeoHeaderName);
  }

  return PROCEED;
}
