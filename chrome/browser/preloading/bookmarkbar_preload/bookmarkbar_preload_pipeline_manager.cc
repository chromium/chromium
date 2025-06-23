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

WEB_CONTENTS_USER_DATA_KEY_IMPL(BookmarkBarPreloadPipelineManager);

BookmarkBarPreloadPipelineManager::BookmarkBarPreloadPipelineManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BookmarkBarPreloadPipelineManager>(
          *web_contents) {}

BookmarkBarPreloadPipelineManager::~BookmarkBarPreloadPipelineManager() =
    default;

// static
BookmarkBarPreloadPipelineManager*
BookmarkBarPreloadPipelineManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  auto* bookmarkbar_preload_manager =
      BookmarkBarPreloadPipelineManager::FromWebContents(web_contents);
  if (!bookmarkbar_preload_manager) {
    BookmarkBarPreloadPipelineManager::CreateForWebContents(web_contents);
    bookmarkbar_preload_manager =
        BookmarkBarPreloadPipelineManager::FromWebContents(web_contents);
  }

  return bookmarkbar_preload_manager;
}

void BookmarkBarPreloadPipelineManager::StartPrerender(const GURL& url) {
  if (pipeline_) {
    // TODO(https://crbug.com/413259638) Adds back the CHECK which checks `url
    // == pipeline_->url()` when the investigation is done. Prerender is
    // expected to be reset when mouseExit happens or every primary page
    // changed, so if a pipeline is present, the url is expected to be the same.
    // But the CHECK is causing https://crbug.com/425612820 unexpectedly, the
    // CHECK is removed at the moment.
    return;
  }

  pipeline_ = std::make_unique<BookmarkBarPreloadPipeline>(url);
  if (!pipeline_->StartPrerender(
          *web_contents(),
          chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar)) {
    pipeline_.reset();
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
  pipeline_.reset();
}
