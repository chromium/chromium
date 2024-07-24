// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/chrome_preloading.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {

bool IsSideSearch(content::BrowserContext* browser_context, const GURL& url) {
  const TemplateURLService* const template_url_service =
      GetTemplateURLServiceFromBrowserContext(browser_context);
  if (!template_url_service)
    return false;

  auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_provider)
    return false;

  return default_search_provider->ContainsSideSearchParam(url) ||
         default_search_provider->ContainsSideImageSearchParam(url);
}

}  // namespace

// Ensure new values do not fall in content internal reserved ranges.
static_assert(
    static_cast<int>(ChromePreloadingEligibility::kMaxValue) <
        static_cast<int>(content::PreloadingEligibility::
                             kPreloadingEligibilityContentStart2) ||
    static_cast<int>(ChromePreloadingEligibility::kMaxValue) >
        static_cast<int>(
            content::PreloadingEligibility::kPreloadingEligibilityContentEnd2));

content::PreloadingEligibility ToPreloadingEligibility(
    ChromePreloadingEligibility eligibility) {
  return static_cast<content::PreloadingEligibility>(eligibility);
}

TemplateURLService* GetTemplateURLServiceFromBrowserContext(
    content::BrowserContext* browser_context) {
  if (Profile* profile = Profile::FromBrowserContext(browser_context)) {
    return TemplateURLServiceFactory::GetForProfile(profile);
  }
  return nullptr;
}

bool HasCanonicalPreloadingOmniboxSearchURL(
    const GURL& preloading_url,
    content::BrowserContext* browser_context,
    GURL* canonical_url) {
  const TemplateURLService* const template_url_service =
      GetTemplateURLServiceFromBrowserContext(browser_context);
  if (!template_url_service) {
    return false;
  }

  auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_provider) {
    return false;
  }

  return default_search_provider->KeepSearchTermsInURL(
      preloading_url, template_url_service->search_terms_data(),
      /*keep_search_intent_params=*/true,
      /*normalize_search_terms=*/true, canonical_url);
}

bool IsSearchDestinationMatch(const GURL& canonical_preloading_search_url,
                              content::BrowserContext* browser_context,
                              const GURL& navigation_url) {
  if (canonical_preloading_search_url.is_empty()) {
    return false;
  }
  // Disable for side search as the formatting is different on those pages.
  if (IsSideSearch(browser_context, navigation_url))
    return false;

  GURL canonical_navigation_url;
  return HasCanonicalPreloadingOmniboxSearchURL(navigation_url, browser_context,
                                                &canonical_navigation_url) &&
         (canonical_preloading_search_url == canonical_navigation_url);
}

bool IsSearchDestinationMatchWithWebUrlMatchResult(
    const GURL& canonical_preloading_search_url,
    content::BrowserContext* browser_context,
    const GURL& navigation_url,
    const std::optional<content::UrlMatchType>& default_web_url_match) {
  return IsSearchDestinationMatch(canonical_preloading_search_url,
                                  browser_context, navigation_url);
}
