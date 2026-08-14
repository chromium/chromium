// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "url/gurl.h"

// static
void BackgroundContentsNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kBlockBackgroundContentsOffExtentNavigation)) {
    return;
  }

  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInMainFrame()) {
    return;
  }

  content::WebContents* const web_contents = handle.GetWebContents();
  if (!web_contents) {
    return;
  }

  BackgroundContentsService* const service =
      BackgroundContentsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service || !service->IsTracked(web_contents)) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<BackgroundContentsNavigationThrottle>(registry));
}

BackgroundContentsNavigationThrottle::BackgroundContentsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

BackgroundContentsNavigationThrottle::~BackgroundContentsNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
BackgroundContentsNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
BackgroundContentsNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* BackgroundContentsNavigationThrottle::GetNameForLogging() {
  return "BackgroundContentsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
BackgroundContentsNavigationThrottle::WillStartOrRedirectRequest() {
  content::WebContents* const web_contents =
      navigation_handle()->GetWebContents();
  Profile* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  BackgroundContentsService* const service =
      BackgroundContentsServiceFactory::GetForProfile(profile);

  const std::string& appid = service->GetParentApplicationId(web_contents);
  if (appid.empty()) {
    return content::NavigationThrottle::PROCEED;
  }

  extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* const extension =
      registry->enabled_extensions().GetByID(appid);
  if (!extension) {
    return content::NavigationThrottle::BLOCK_REQUEST;
  }

  const GURL& url = navigation_handle()->GetURL();
  if (url.is_empty() || url.SchemeIs(url::kAboutScheme)) {
    return content::NavigationThrottle::PROCEED;
  }

  if (!extension->web_extent().MatchesURL(url)) {
    return content::NavigationThrottle::BLOCK_REQUEST;
  }

  return content::NavigationThrottle::PROCEED;
}
