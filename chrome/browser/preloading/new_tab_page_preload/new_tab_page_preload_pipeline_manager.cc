// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(NewTabPagePreloadPipelineManager);

NewTabPagePreloadPipelineManager::NewTabPagePreloadPipelineManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NewTabPagePreloadPipelineManager>(
          *web_contents) {}

NewTabPagePreloadPipelineManager::~NewTabPagePreloadPipelineManager() = default;

// static
NewTabPagePreloadPipelineManager*
NewTabPagePreloadPipelineManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  auto* new_tab_page_preload_manager =
      NewTabPagePreloadPipelineManager::FromWebContents(web_contents);
  if (!new_tab_page_preload_manager) {
    NewTabPagePreloadPipelineManager::CreateForWebContents(web_contents);
    new_tab_page_preload_manager =
        NewTabPagePreloadPipelineManager::FromWebContents(web_contents);
  }

  return new_tab_page_preload_manager;
}

bool NewTabPagePreloadPipelineManager::StartPrerender(
    const GURL& url,
    content::PreloadingPredictor predictor) {
  if (pipeline_) {
    // TODO(crbug.com/421941586): Introduce a CHECK here to ensure the pipeline
    // contains the same url.
    // Prerender is expected to be reset when mouseExit happens or every primary
    // page changed, so if a pipeline is present, this is going to be a
    // duplicate attempt.
    return true;
  }

  pipeline_ = std::make_unique<NewTabPagePreloadPipeline>(url);
  if (!pipeline_->StartPrerender(*web_contents(), predictor)) {
    pipeline_.reset();
  }

  return pipeline_ != nullptr;
}

void NewTabPagePreloadPipelineManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // This is a primary page change. Reset the preload pipeline.
  pipeline_.reset();
}

void NewTabPagePreloadPipelineManager::ResetPrerender() {
  pipeline_.reset();
}
