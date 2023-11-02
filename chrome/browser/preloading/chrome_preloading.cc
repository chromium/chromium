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

using content::PreloadingPredictor;

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

content::PreloadingPredictor ToPreloadingPredictor(
    ChromePreloadingPredictor predictor) {
  return static_cast<content::PreloadingPredictor>(predictor);
}

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

std::u16string ExtractSearchTermsFromURL(
    const TemplateURLService* const template_url_service,
    const GURL& url) {
  // Can be nullptr in unit tests.
  if (!template_url_service) {
    return std::u16string();
  }
  auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search_provider);
  std::u16string matched_search_terms;
  default_search_provider->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &matched_search_terms);
  return matched_search_terms;
}

std::u16string ExtractSearchTermsFromURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  const TemplateURLService* const template_url_service =
      GetTemplateURLServiceFromBrowserContext(browser_context);
  return ExtractSearchTermsFromURL(template_url_service, url);
}

// Returns true when the two given URLs are considered as navigating to the same
// search term.
bool IsSearchDestinationMatch(const std::u16string& preloading_search_terms,
                              content::BrowserContext* browser_context,
                              const GURL& navigation_url) {
  // Return false in case search_terms are empty as we only match with valid
  // search terms.
  if (preloading_search_terms.empty())
    return false;

  // Disable for side search as the formatting is different on those pages.
  if (IsSideSearch(browser_context, navigation_url))
    return false;

  std::u16string matched_search_terms =
      ExtractSearchTermsFromURL(browser_context, navigation_url);
  return matched_search_terms == preloading_search_terms;
}
