// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/dse_prewarm_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"

// static
void DSEPrewarmNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(features::kPrewarm) ||
      !features::kPrewarmThrottleUserNavigation.Get()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<DSEPrewarmNavigationThrottle>(registry));
}

DSEPrewarmNavigationThrottle::DSEPrewarmNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {
  auto* web_contents = navigation_handle()->GetWebContents();
  CHECK(web_contents);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return;
  }

  dse_url_ = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data());
}

DSEPrewarmNavigationThrottle::~DSEPrewarmNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
DSEPrewarmNavigationThrottle::WillStartRequest() {
  auto* web_contents = navigation_handle()->GetWebContents();
  CHECK(web_contents);

  if (url::Origin::Create(navigation_handle()->GetURL()) !=
      url::Origin::Create(dse_url_)) {
    return PROCEED;
  }

  // For DSEPrewarm navigation itself, `WillStartReqest` is called before
  // SearchPrewarmProgressServiceFactory tracks the prewarm page so it will
  // not be blocked.
  // TODO(crbug.com/485414743): Add check logic to verify the on-going
  // navigation is not the prewarm page.
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* service = SearchPrewarmProgressServiceFactory::GetForProfile(profile);
  if (service && service->HasOnGoingSearchPrewarm()) {
    service->AddSearchPrewarmFinishedCallback(
        base::BindOnce(&DSEPrewarmNavigationThrottle::OnSearchPrewarmFinished,
                       weak_factory_.GetWeakPtr()));
    return DEFER;
  }

  return PROCEED;
}

const char* DSEPrewarmNavigationThrottle::GetNameForLogging() {
  return "DSEPrewarmNavigationThrottle";
}

void DSEPrewarmNavigationThrottle::OnSearchPrewarmFinished() {
  auto* web_contents = navigation_handle()->GetWebContents();
  // The callback can be called during the destruction of the web contents.
  // In this case, the navigation is no longer required, triggering resume
  // will case CHECK failures.
  if (web_contents && !web_contents->IsBeingDestroyed()) {
    Resume();
  }
}
