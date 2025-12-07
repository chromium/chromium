// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Roles:
//
// - Manages preload pipelines per WebContents.
class NewTabPagePreloadPipelineManager : public content::WebContentsObserver {
 public:
  explicit NewTabPagePreloadPipelineManager(content::WebContents* contents);
  ~NewTabPagePreloadPipelineManager() override;

  // Not movable nor copyable.
  NewTabPagePreloadPipelineManager(const NewTabPagePreloadPipelineManager&&) =
      delete;
  NewTabPagePreloadPipelineManager& operator=(
      const NewTabPagePreloadPipelineManager&&) = delete;
  NewTabPagePreloadPipelineManager(const NewTabPagePreloadPipelineManager&) =
      delete;
  NewTabPagePreloadPipelineManager& operator=(
      const NewTabPagePreloadPipelineManager&) = delete;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  static NewTabPagePreloadPipelineManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  void StartPrefetch(const GURL& url);

  void StartPrerender(const GURL& url, content::PreloadingPredictor predictor);

  void ResetPrerender();

 private:
  // Resets the pipeline to allow another preloading attempt if a given url is
  // different from the started one. Pipeline creation will follow the check if
  // a pipeline hasn't existed.
  void EnsurePipelineForUrl(const GURL& url);

  std::unique_ptr<NewTabPagePreloadPipeline> pipeline_;
};

#endif  // CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_MANAGER_H_
