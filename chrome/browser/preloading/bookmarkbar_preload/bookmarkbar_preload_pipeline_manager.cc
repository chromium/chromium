// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline_manager.h"

#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

BookmarkBarPreloadPipelineManager::BookmarkBarPreloadPipelineManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

BookmarkBarPreloadPipelineManager::~BookmarkBarPreloadPipelineManager() =
    default;

void BookmarkBarPreloadPipelineManager::StartPrefetch(const GURL& url) {
  EnsurePipelineForUrl(url);
  pipeline_->StartPrefetch(
      *web_contents(),
      chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);

  if (on_prefetch_completed_or_failed_for_testing_) {
    pipeline_->SetOnPrefetchCompletedOrFailedCallbackForTesting(  // IN-TEST
        std::move(on_prefetch_completed_or_failed_for_testing_));
  }
}

void BookmarkBarPreloadPipelineManager::StartPrerender(const GURL& url) {
  EnsurePipelineForUrl(url);
  pipeline_->StartPrerender(
      *web_contents(),
      chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
}

void BookmarkBarPreloadPipelineManager::EnsurePipelineForUrl(const GURL& url) {
  if (pipeline_ && pipeline_->url() != url) {
    pipeline_.reset();
  }

  if (!pipeline_) {
    pipeline_ = std::make_unique<BookmarkBarPreloadPipeline>(url);
  }
}

void BookmarkBarPreloadPipelineManager::
    SetOnPrefetchCompletedOrFailedCallbackForTesting(
        base::RepeatingCallback<
            void(const network::URLLoaderCompletionStatus& completion_status,
                 const std::optional<int>& response_code)>
            on_prefetch_completed_or_failed) {
  if (pipeline_) {
    pipeline_->SetOnPrefetchCompletedOrFailedCallbackForTesting(  // IN-TEST
        on_prefetch_completed_or_failed);
  } else {
    // This callback is used in tests only. Since the test triggers
    // preloading by mouse events, and it is currently no way to sync between
    // setting the callback and triggering preloading. If pipeline hasn't been
    // triggered yet, buffer the callback function for the upcoming trigger.
    // This is necessary for
    // PreloadBookmarkBarPrefetchEnabledPrerenderEnabledNavigationTest.*
    on_prefetch_completed_or_failed_for_testing_ =
        std::move(on_prefetch_completed_or_failed);
  }
}

void BookmarkBarPreloadPipelineManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // This is a primary page change. Reset the preload pipeline.
  pipeline_.reset();
}

void BookmarkBarPreloadPipelineManager::ResetPrerender() {
  if (pipeline_) {
    pipeline_->ResetPrerender();
  }
}
