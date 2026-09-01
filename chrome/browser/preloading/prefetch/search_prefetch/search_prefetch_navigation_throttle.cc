// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_navigation_throttle.h"

#include <memory>

#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"

// static
void SearchPrefetchNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInMainFrame()) {
    return;
  }
  content::WebContents* web_contents = handle.GetWebContents();
  if (!web_contents) {
    return;
  }
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }
  auto* service = SearchPrefetchServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  registry.AddThrottle(std::make_unique<SearchPrefetchNavigationThrottle>(
      registry, service->GetWeakPtr()));
}

SearchPrefetchNavigationThrottle::SearchPrefetchNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    base::WeakPtr<SearchPrefetchService> search_prefetch_service)
    : content::NavigationThrottle(registry),
      search_prefetch_service_(std::move(search_prefetch_service)),
      navigation_id_(navigation_handle()->GetNavigationId()) {}

SearchPrefetchNavigationThrottle::~SearchPrefetchNavigationThrottle() {
  if (search_prefetch_service_) {
    search_prefetch_service_->RemoveServingNavigationId(navigation_id_);
  }
}

content::NavigationThrottle::ThrottleCheckResult
SearchPrefetchNavigationThrottle::WillProcessResponse() {
  if (search_prefetch_service_ &&
      search_prefetch_service_->IsServingNavigation(navigation_id_)) {
    MarkNavigationServedBySearchPrefetch(*navigation_handle());
  }
  return content::NavigationThrottle::PROCEED;
}

const char* SearchPrefetchNavigationThrottle::GetNameForLogging() {
  return "SearchPrefetchNavigationThrottle";
}
