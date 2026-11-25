// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gemini_app/gemini_app_tab_helper.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

using Page = GeminiAppTabHelper::Page;

// Maps page URL hash metric names to their corresponding `Page`.
constexpr auto kPageHashMetricNames = base::MakeFixedFlatMap<uint64_t, Page>(
    {{15434391541687473744u, Page::kCongratulations},
     {2639084485652816410u, Page::kTermsAndConditionsStandard},
     {8933819972841556021u, Page::kTermsAndConditionsStandard},
     {11887379483153592206u, Page::kTermsAndConditionsStandard},
     {6579551706563083045u, Page::kOffer},
     {9605163350832310418u, Page::kTermsAndConditionsCBX},
     {14050260147306734198u, Page::kTermsAndConditionsCBX},
     {18084016612939108325u, Page::kTermsAndConditionsCBX}});

// Optional override for `kPageHashMetricNames`, set only in tests.
constinit std::optional<base::span<const std::pair<GURL, Page>>>
    g_page_urls_for_testing = std::nullopt;

// Returns the `Page` associated with `page_hash`, or `std::nullopt` if not
// found.
std::optional<Page> FindPage(uint64_t page_hash) {
  if (g_page_urls_for_testing.has_value()) [[unlikely]] {
    CHECK_IS_TEST();
    for (const auto& [url, page] : *g_page_urls_for_testing) {
      if (base::HashMetricName(url.spec()) == page_hash) {
        return page;
      }
    }
    return std::nullopt;
  }
  if (auto it = kPageHashMetricNames.find(page_hash);
      it != kPageHashMetricNames.end()) {
    return it->second;
  }
  return std::nullopt;
}

// Returns whether the specified `web_contents` is off the record.
bool IsOffTheRecord(content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  return browser_context && browser_context->IsOffTheRecord();
}

}  // namespace

// GeminiAppTabHelper ----------------------------------------------------------

GeminiAppTabHelper::GeminiAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

GeminiAppTabHelper::~GeminiAppTabHelper() = default;

// static
std::unique_ptr<GeminiAppTabHelper> GeminiAppTabHelper::MaybeCreate(
    content::WebContents* web_contents) {
  if (!IsOffTheRecord(web_contents)) {
    return base::WrapUnique(new GeminiAppTabHelper(web_contents));
  }
  return nullptr;
}

// static
base::AutoReset<
    std::optional<base::span<const std::pair<GURL, GeminiAppTabHelper::Page>>>>
GeminiAppTabHelper::SetPageUrlsForTesting(
    base::span<const std::pair<GURL, Page>> page_urls) {
  return {&g_page_urls_for_testing, page_urls};
}

void GeminiAppTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Check for exact page match.
  std::optional<Page> page =
      FindPage(base::HashMetricName(navigation_handle->GetURL().spec()));

  // Check for page match w/o filename.
  if (!page) {
    page = FindPage(base::HashMetricName(
        navigation_handle->GetURL().GetWithoutFilename().spec()));
  }

  // Record page visit.
  if (page) {
    base::UmaHistogramEnumeration("Ash.GeminiApp.Page.Visit", *page);
  }
}
