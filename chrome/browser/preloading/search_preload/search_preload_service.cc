// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/search_preload/search_preload_service.h"

#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"

// static
SearchPreloadService* SearchPreloadService::GetForProfile(Profile* profile) {
  return SearchPreloadServiceFactory::GetForProfile(profile);
}

SearchPreloadService::SearchPreloadService(Profile* profile)
    : profile_(profile) {
  CHECK(features::IsDsePreload2Enabled());

  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  CHECK(template_url_service);
  observer_.Observe(template_url_service);
}

SearchPreloadService::~SearchPreloadService() = default;

void SearchPreloadService::Shutdown() {
  ClearPreloads();
  observer_.Reset();
}

void SearchPreloadService::OnTemplateURLServiceChanged() {
  ClearPreloads();
}

void SearchPreloadService::ClearPreloads() {
  if (pipeline_manager_.has_value() && pipeline_manager_.value()) {
    pipeline_manager_.value()->ClearPreloads();
  }
  pipeline_manager_.reset();
}

SearchPreloadPipelineManager&
SearchPreloadService::GetOrCreatePipelineManagerWithLimit(
    content::WebContents& web_contents) {
  // Allow at most one WebContents to hold preloads.
  //
  // TODO(crbug.com/394213503): Reconsider the limitation.
  const bool is_occupied_with_given_web_contents =
      pipeline_manager_.has_value() && pipeline_manager_.value() &&
      &pipeline_manager_.value()->GetWebContents() == &web_contents;
  if (!is_occupied_with_given_web_contents) {
    ClearPreloads();
  }

  if (!pipeline_manager_.has_value()) {
    SearchPreloadPipelineManager::CreateForWebContents(&web_contents);
    auto* pipeline_manager =
        SearchPreloadPipelineManager::FromWebContents(&web_contents);
    CHECK(pipeline_manager);
    pipeline_manager_ = pipeline_manager->GetWeakPtr();
  }

  return *pipeline_manager_.value();
}

void SearchPreloadService::OnAutocompleteResultChanged(
    content::WebContents* web_contents,
    const AutocompleteResult& result) {
  if (!web_contents) {
    return;
  }

  GetOrCreatePipelineManagerWithLimit(*web_contents)
      .OnAutocompleteResultChanged(*profile_, result);
}

bool SearchPreloadService::OnNavigationLikely(
    size_t index,
    const AutocompleteMatch& match,
    omnibox::mojom::NavigationPredictor navigation_predictor,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  return GetOrCreatePipelineManagerWithLimit(*web_contents)
      .OnNavigationLikely(*profile_, match, navigation_predictor);
}
