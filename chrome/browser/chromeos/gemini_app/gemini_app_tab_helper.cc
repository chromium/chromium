// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gemini_app/gemini_app_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// Returns pages mapped to their hash metric names.
std::map<uint64_t, GeminiAppTabHelper::Page>* GetPageHashMetricNames() {
  using Page = GeminiAppTabHelper::Page;
  static base::NoDestructor<std::map<uint64_t, Page>> page_hash_metric_names(
      {{15434391541687473744u, Page::kCongratulations},
       {2639084485652816410u, Page::kTermsAndConditionsStandard},
       {8933819972841556021u, Page::kTermsAndConditionsStandard},
       {11887379483153592206u, Page::kTermsAndConditionsStandard},
       {6579551706563083045u, Page::kOffer},
       {9605163350832310418u, Page::kTermsAndConditionsCBX},
       {14050260147306734198u, Page::kTermsAndConditionsCBX},
       {18084016612939108325u, Page::kTermsAndConditionsCBX}});
  return page_hash_metric_names.get();
}

// Returns whether the specified `web_contents` is off the record.
bool IsOffTheRecord(content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  return browser_context && browser_context->IsOffTheRecord();
}

}  // namespace

// GeminiAppTabHelper ----------------------------------------------------------

GeminiAppTabHelper::GeminiAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<GeminiAppTabHelper>(*web_contents) {}

GeminiAppTabHelper::~GeminiAppTabHelper() = default;

// static
void GeminiAppTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (chromeos::features::IsGeminiAppPreinstallEnabled() &&
      !IsOffTheRecord(web_contents)) {
    GeminiAppTabHelper::CreateForWebContents(web_contents);
  }
}

// static
base::AutoReset<std::map<uint64_t, GeminiAppTabHelper::Page>>
GeminiAppTabHelper::SetPageUrlsForTesting(std::map<GURL, Page> page_urls) {
  std::map<uint64_t, Page> page_hash_metric_names;
  for (const auto& [url, page] : page_urls) {
    page_hash_metric_names.emplace(base::HashMetricName(url.spec()), page);
  }
  return {GetPageHashMetricNames(), std::move(page_hash_metric_names)};
}

void GeminiAppTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  const std::map<uint64_t, GeminiAppTabHelper::Page>* page_hash_metric_names =
      GetPageHashMetricNames();

  // Check for exact page match.
  auto it = page_hash_metric_names->find(
      base::HashMetricName(navigation_handle->GetURL().spec()));

  // Check for page match w/o filename.
  if (it == page_hash_metric_names->end()) {
    it = page_hash_metric_names->find(base::HashMetricName(
        navigation_handle->GetURL().GetWithoutFilename().spec()));
  }

  // Record page visit.
  if (it != page_hash_metric_names->end()) {
    base::UmaHistogramEnumeration("Ash.GeminiApp.Page.Visit",
                                  /*page=*/it->second);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(GeminiAppTabHelper);
