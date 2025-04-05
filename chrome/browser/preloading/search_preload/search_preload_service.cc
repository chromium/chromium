// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_service.h"

#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"

// static
SearchPreloadService* SearchPreloadService::GetForProfile(Profile* profile) {
  return SearchPreloadServiceFactory::GetForProfile(profile);
}

SearchPreloadService::SearchPreloadService(Profile* profile)
    : profile_(profile) {
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  CHECK(template_url_service);
  observer_.Observe(template_url_service);
}

SearchPreloadService::~SearchPreloadService() = default;

void SearchPreloadService::Shutdown() {
  observer_.Reset();
}

void SearchPreloadService::OnTemplateURLServiceChanged() {
  ClearPreloads();
}

void SearchPreloadService::ClearPreloads() {
  NOTIMPLEMENTED();
}

void SearchPreloadService::OnAutocompleteResultChanged(
    content::WebContents* web_contents,
    const AutocompleteResult& result) {
  NOTIMPLEMENTED();
}

bool SearchPreloadService::OnNavigationLikely(
    size_t index,
    const AutocompleteMatch& match,
    omnibox::mojom::NavigationPredictor navigation_predictor,
    content::WebContents* web_contents) {
  NOTREACHED();
}
