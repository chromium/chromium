// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

NewTabPagePreloadPipelineManager::NewTabPagePreloadPipelineManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

NewTabPagePreloadPipelineManager::~NewTabPagePreloadPipelineManager() = default;

void NewTabPagePreloadPipelineManager::StartPrefetch(const GURL& url) {
  EnsurePipelineForUrl(url);
  pipeline_->StartPrefetch(
      *web_contents(), chrome_preloading_predictor::kMouseHoverOnNewTabPage);
}

void NewTabPagePreloadPipelineManager::StartPrerender(
    const GURL& url,
    content::PreloadingPredictor predictor) {
  EnsurePipelineForUrl(url);
  pipeline_->StartPrerender(*web_contents(), predictor);
}

void NewTabPagePreloadPipelineManager::EnsurePipelineForUrl(const GURL& url) {
  if (pipeline_ && pipeline_->url() != url) {
    pipeline_.reset();
  }

  if (!pipeline_) {
    pipeline_ = std::make_unique<NewTabPagePreloadPipeline>(url);
  }
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
