// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
#define CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_

#include <string>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/preloading.h"

#include "url/gurl.h"

class TemplateURLService;

// If you change any of the following enums or static variables, please follow
// the process in go/preloading-dashboard-updates to update the mapping
// reflected in dashboard, or if you are not a Googler, please file an FYI bug
// on https://crbug.new with component Internals>Preload.

// Defines various embedder triggering mechanisms which triggers different
// preloading operations mentioned in //content/public/browser/preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Advance numbering by +1 when adding a new element.
//
// Please make sure Chrome `PreloadingPredictor` are defined after 100
// (inclusive) as 99 and below are reserved for content-public and
// content-internal definitions. Both the value and the name should be unique
// across all the namespaces.
//
// LINT.IfChange
namespace chrome_preloading_predictor {
// When the preloading URL is predicted from the Omnibox Direct URL Input
// (DUI). This is used to perform various preloading operations like prefetch
// and prerender to load Omnibox predicted URLs faster.
static constexpr content::PreloadingPredictor kOmniboxDirectURLInput(
    100,
    "OmniboxDirectURLInput");

// When a pointerdown (e.g. mousedown or touchstart) event happens on an
// anchor element with an href value pointing to an HTTP(S) origin, we may
// attempt to preload the link.
static constexpr content::PreloadingPredictor kPointerDownOnAnchor(
    101,
    "PointerDownOnAnchor");

// When the preloading URL is predicted from the default search suggest
// service for faster search page loads.
static constexpr content::PreloadingPredictor kDefaultSearchEngine(
    102,
    "DefaultSearchEngine");

// When the preloading URL is predicted from the default search suggest due to
// change in Omnibox selection.
static constexpr content::PreloadingPredictor kOmniboxSearchPredictor(
    103,
    "OmniboxSearchPredictor");

// When the preloading URL is predicted from the default search suggest due to
// mouse being pressed down on a Omnibox Search suggestion.
static constexpr content::PreloadingPredictor kOmniboxMousePredictor(
    104,
    "OmniboxMousePredictor");

// When the default match in omnibox has the search prefetch or prerender
// hint.
static constexpr content::PreloadingPredictor kOmniboxSearchSuggestDefaultMatch(
    105,
    "OmniboxSearchSuggestDefaultMatch");

// When the user hovers their mouse over the back button.
static constexpr content::PreloadingPredictor kBackButtonHover(
    106,
    "BackButtonHover");

// When a pointerdown (e.g. mousedown or touchstart) event happens on an
// bookmark bar link to an HTTPS origin, we may attempt to preload the link.
static constexpr content::PreloadingPredictor kPointerDownOnBookmarkBar(
    107,
    "PointerDownOnBookmarkBar");

// When a mousehover event happens on a bookmark bar link to an HTTPS origin,
// we may attempt to preload the link.
static constexpr content::PreloadingPredictor kMouseHoverOnBookmarkBar(
    108,
    "MouseHoverOnBookmarkBar");

// When a pointerdown (e.g. mousedown or touchstart) event happens on a
// new tab page link to an HTTPS origin, we may attempt to preload the link.
static constexpr content::PreloadingPredictor kPointerDownOnNewTabPage(
    109,
    "PointerDownOnNewTabPage");

// When a mousehover event happens on a new tab page link to an HTTPS origin,
// we may attempt to preload the link.
static constexpr content::PreloadingPredictor kMouseHoverOnNewTabPage(
    110,
    "MouseHoverOnNewTabPage");

// When the preloading URL is predicted from the default search suggest due to
// the user touching down on a Omnibox Search suggestion.
static constexpr content::PreloadingPredictor kOmniboxTouchDownPredictor(
    111,
    "OmniboxTouchDownPredirector");

// When the Link-Preview loads a page with prerendering infrastractures.
// TODO(b:291867362): This is not used by the current implementation, but might
// be reused in the future.
static constexpr content::PreloadingPredictor kLinkPreview(112, "LinkPreview");

// When a mousehover or mousedown event happens on a bookmark bar linking to an
// HTTPS origin, we may attempt to preload the link. This predictor, instead of
// using kPointerDownOnBookmarkBar or kMouseHoverOnBookmarkBar, is for solving
// the problem in https://crbug.com/1516514.
static constexpr content::PreloadingPredictor
    kMouseHoverOrMouseDownOnBookmarkBar(113,
                                        "MouseHoverOrMouseDownOnBookmarkBar");

// When a touch event happens on a new tab page link to an HTTPS origin,
// we may attempt to preload the link.
static constexpr content::PreloadingPredictor kTouchOnNewTabPage(
    114,
    "TouchOnNewTabPage");

// When a certain CCT prefetch API is triggered.
static constexpr content::PreloadingPredictor kChromeCustomTabs(
    115,
    "ChromeCustomTabs");
}  // namespace chrome_preloading_predictor
// LINT.ThenChange()

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
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

  kMaxValue = kPreloadingErrorBackOff,
};
// LINT.ThenChange()

// Helper method to convert ChromePreloadingEligibility to
// content::PreloadingEligibility to avoid casting.
content::PreloadingEligibility ToPreloadingEligibility(
    ChromePreloadingEligibility eligibility);

// Helpers methods to extract search terms from a given URL.
TemplateURLService* GetTemplateURLServiceFromBrowserContext(
    content::BrowserContext* browser_context);
std::u16string ExtractSearchTermsFromURL(
    const TemplateURLService* const template_url_service,
    const GURL& url);
std::u16string ExtractSearchTermsFromURL(
    content::BrowserContext* browser_context,
    const GURL& url);

// Returns true if a canonical URL representation of a |preloading_url| can be
// generated. |canonical_url| is set to the canonical URL representation when
// this method returns |true|.
bool HasCanonicalPreloadingOmniboxSearchURL(
    const GURL& preloading_url,
    content::BrowserContext* browser_context,
    GURL* canonical_url);

// Returns true when |navigation_url| is considered as navigating to the same
// omnibox search results page as |canonical_preloading_search_url|.
bool IsSearchDestinationMatch(const GURL& canonical_preloading_search_url,
                              content::BrowserContext* browser_context,
                              const GURL& navigation_url);
// Returns true when |navigation_url| is considered as navigating to the same
// omnibox search results page as |canonical_preloading_search_url|. Includes
// the result from the default web url match operation.
bool IsSearchDestinationMatchWithWebUrlMatchResult(
    const GURL& canonical_preloading_search_url,
    content::BrowserContext* browser_context,
    const GURL& navigation_url,
    const std::optional<content::UrlMatchType>& default_web_url_match =
        std::nullopt);

#endif  // CHROME_BROWSER_PRELOADING_CHROME_PRELOADING_H_
