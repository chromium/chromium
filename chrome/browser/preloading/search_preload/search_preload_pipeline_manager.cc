// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_pipeline_manager.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_pipeline.h"
#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"

namespace {

// Ergonomic wrapper of `HasCanonicalPreloadingOmniboxSearchURL()`
std::optional<GURL> GetCanonicalUrlForSearchPreload(
    content::BrowserContext& browser_context,
    const GURL& preload_url) {
  GURL canonical_url;
  if (HasCanonicalPreloadingOmniboxSearchURL(preload_url, &browser_context,
                                             &canonical_url)) {
    return canonical_url;
  }

  return std::nullopt;
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchPreloadPipelineManager);

SearchPreloadPipelineManager::SearchPreloadPipelineManager(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SearchPreloadPipelineManager>(
          *web_contents) {}

SearchPreloadPipelineManager::~SearchPreloadPipelineManager() = default;

void SearchPreloadPipelineManager::ClearPreloads() {
  pipelines_.clear();
}

void SearchPreloadPipelineManager::OnAutocompleteResultChanged(
    Profile& profile,
    const AutocompleteResult& result) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  CHECK(template_url_service);
  if (!template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // TODO(crbug.com/409457832): Allow preloads for non-default match with
  // feature flag.
  if (!result.default_match()) {
    return;
  }
  const auto& match = *result.default_match();

  const bool should_prefetch = BaseSearchProvider::ShouldPrefetch(match) ||
                               BaseSearchProvider::ShouldPrerender(match);
  const bool should_prerender = BaseSearchProvider::ShouldPrerender(match);

  // In the case of Default Search Engine Prediction, the confidence depends
  // on the type of preload. For prerender requests, the confidence is
  // comparatively higher than the prefetch to avoid the impact of wrong
  // predictions. We set confidence as 80 for prerender matches and 60 for
  // prefetch as an approximate number to differentiate both these cases.
  //
  // The value is used only for precog. So, these values have no concreate
  // meanings.
  int confidence;
  if (should_prerender) {
    confidence = 80;
  } else if (should_prefetch) {
    confidence = 60;
  } else {
    return;
  }

  std::optional<GURL> maybe_canonical_url =
      GetCanonicalUrlForSearchPreload(profile, match.destination_url);
  if (!maybe_canonical_url.has_value()) {
    return;
  }
  const GURL& canonical_url = maybe_canonical_url.value();

  // TODO(crbug.com/403198750): Limit the number of active pipelines.
  if (!pipelines_.contains(canonical_url)) {
    pipelines_.insert_or_assign(
        canonical_url, std::make_unique<SearchPreloadPipeline>(canonical_url));
  }
  pipelines_[canonical_url]->UpdateConfidence(GetWebContents(), confidence);

  CHECK(should_prefetch);
  const GURL prefetch_url =
      GetPrefetchUrlFromMatch(*match.search_terms_args, *template_url_service,
                              /*is_navigation_likely=*/false);
  pipelines_[canonical_url]->StartPrefetch(
      GetWebContents(), prefetch_url,
      chrome_preloading_predictor::kDefaultSearchEngine);

  // Trigger prerender without waiting prefetch.
  //
  // They are coordinated by `PrefetchMatchResolver`. For more details, see
  // https://docs.google.com/document/d/1IAIVrDBE-FnO14Qnghr8hsrxUeoFfeob5QIsV_UNRck/edit?tab=t.0#heading=h.vpxgrp4zne09
  if (should_prerender) {
    const GURL prerender_url = GetPrerenderUrlFromMatch(
        *match.search_terms_args, *template_url_service);
    pipelines_[canonical_url]->StartPrerender(
        GetWebContents(), prerender_url,
        chrome_preloading_predictor::kDefaultSearchEngine);
  }
}
