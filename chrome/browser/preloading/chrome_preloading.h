// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
#define CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_

#include <string>

#include "content/public/browser/preloading.h"

#include "url/gurl.h"

class TemplateURLService;
using content::PreloadingPredictor;
namespace content {
class WebContents;
}

// Defines various embedder triggering mechanisms which triggers different
// preloading operations mentioned in //content/public/browser/preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromePreloadingPredictor {
  // Numbering starts from `kPreloadingPredictorContentEnd` defined in
  // //content/browser/public/preloading.h . Advance numbering by +1 when adding
  // a new element.

  // When the preloading URL is predicted from the Omnibox Direct URL Input
  // (DUI). This is used to perform various preloading operations like prefetch
  // and prerender to load Omnibox predicted URLs faster.
  kOmniboxDirectURLInput =
      static_cast<int>(PreloadingPredictor::kPreloadingPredictorContentEnd),

  // When a pointerdown (e.g. mousedown or touchstart) event happens on an
  // anchor element with an href value pointing to an HTTP(S) origin, we may
  // attempt to preload the link.
  kPointerDownOnAnchor =
      static_cast<int>(PreloadingPredictor::kPreloadingPredictorContentEnd) + 1,

  // When the preloading URL is predicted from the default search suggest
  // service for faster search page loads.
  kDefaultSearchEngine =
      static_cast<int>(PreloadingPredictor::kPreloadingPredictorContentEnd) + 2,

  // When the preloading URL is predicted from the default search suggest due to
  // change in Omnibox selection.
  kOmniboxSearchPredictor =
      static_cast<int>(PreloadingPredictor::kPreloadingPredictorContentEnd) + 3,

  // TODO(crbug.com/1309934): Integrate more Preloading predictors with
  // Preloading logging APIs.
};

// Helper method to convert ChromePreloadingPredictor to
// content::PreloadingPredictor to avoid casting.
content::PreloadingPredictor ToPreloadingPredictor(
    ChromePreloadingPredictor predictor);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromePreloadingEligibility {
  // Numbering starts from `kPreloadingEligibilityContentEnd` defined in
  // //content/public/preloading.h . Advance numbering by +1 when adding a new
  // element.

  // Chrome was unable to get a LoadingPredictor object for the user profile.
  kUnableToGetLoadingPredictor = static_cast<int>(
      content::PreloadingEligibility::kPreloadingEligibilityContentEnd),

  // Preloading was ineligible because Prefetch was not started and Prerender
  // can't be triggered.
  kPrefetchNotStarted =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      1,

  // Preloading was ineligible because Prefetch failed and Prerender can't be
  // triggered.
  kPrefetchFailed =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      2,

  // Preloading was ineligible because Prerender was already consumed and can't
  // be triggered again.
  kPrerenderConsumed =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      3,

  // Preloading was ineligible because the default search engine was not set.
  kSearchEngineNotValid =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      4,

  // Preloading can't be started because there are no search terms present.
  kNoSearchTerms =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      5,

  // Preloading was ineligible due to error in the network request.
  kPreloadingErrorBackOff =
      static_cast<int>(
          content::PreloadingEligibility::kPreloadingEligibilityContentEnd) +
      6,
};

// Helper method to convert ChromePreloadingEligibility to
// content::PreloadingEligibility to avoid casting.
content::PreloadingEligibility ToPreloadingEligibility(
    ChromePreloadingEligibility eligibility);

// Helpers methods to extract search terms from a given URL.
TemplateURLService* GetTemplateURLServiceFromWebContents(
    content::WebContents& web_contents);
std::u16string ExtractSearchTermsFromURL(
    const TemplateURLService* const template_url_service,
    const GURL& url);
std::u16string ExtractSearchTermsFromURL(content::WebContents& web_contents,
                                         const GURL& url);

// Returns true when the two given URLs are considered as navigating to the same
// search term.
bool IsSearchDestinationMatch(const std::u16string& preloading_search_terms,
                              content::WebContents& web_contents,
                              const GURL& navigation_url);

#endif  // CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
