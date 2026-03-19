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
  auto* profile = Profile::FromBrowserContext(
      registry.GetNavigationHandle().GetWebContents()->GetBrowserContext());
  auto* prewarm_progress_service =
      SearchPrewarmProgressServiceFactory::GetForProfile(profile);
  if (!prewarm_progress_service) {
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

  auto* prewarm_progress_service =
      SearchPrewarmProgressServiceFactory::GetForProfile(profile);
  CHECK(prewarm_progress_service);
  prewarm_progress_service_ = prewarm_progress_service->GetWeakPtr();
  prewarm_finished_subscription_ =
      prewarm_progress_service_->RegisterSearchPrewarmFinishedCallback(
          base::BindRepeating(
              &DSEPrewarmNavigationThrottle::OnSearchPrewarmFinished,
              weak_factory_.GetWeakPtr()));

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
  return CheckNoRaceWithDSEPrewarm();
}

content::NavigationThrottle::ThrottleCheckResult
DSEPrewarmNavigationThrottle::WillRedirectRequest() {
  return CheckNoRaceWithDSEPrewarm();
}

content::NavigationThrottle::ThrottleCheckResult
DSEPrewarmNavigationThrottle::CheckNoRaceWithDSEPrewarm() {
  auto* web_contents = navigation_handle()->GetWebContents();
  CHECK(web_contents);

  if (url::Origin::Create(navigation_handle()->GetURL()) !=
      url::Origin::Create(dse_url_)) {
    return PROCEED;
  }

  if (!prewarm_progress_service_) {
    return PROCEED;
  }

  content::PrerenderHostId host_id = navigation_handle()->GetPrerenderHostId();
  if (host_id && prewarm_progress_service_->IsOnGoingSearchPrewarm(host_id)) {
    // If this navigation itself is an ongoing prewarm, we shouldn't throttle
    // it.
    return PROCEED;
  }

  if (prewarm_progress_service_->HasOnGoingSearchPrewarm()) {
    is_deferring_ = true;
    return DEFER;
  }

  return PROCEED;
}

const char* DSEPrewarmNavigationThrottle::GetNameForLogging() {
  return "DSEPrewarmNavigationThrottle";
}

void DSEPrewarmNavigationThrottle::OnSearchPrewarmFinished() {
  if (!is_deferring_) {
    return;
  }
  is_deferring_ = false;

  auto* web_contents = navigation_handle()->GetWebContents();
  // The callback can be called during the destruction of the web contents.
  // In this case, the navigation is no longer required, triggering resume
  // will case CHECK failures.
  if (web_contents && !web_contents->IsBeingDestroyed()) {
    Resume();
  }
}
