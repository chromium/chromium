// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Roles:
//
// - Manages preload pipelines per WebContents.
class BookmarkBarPreloadPipelineManager : public content::WebContentsObserver {
 public:
  explicit BookmarkBarPreloadPipelineManager(
      content::WebContents* web_contents);
  ~BookmarkBarPreloadPipelineManager() override;

  // Not movable nor copyable.
  BookmarkBarPreloadPipelineManager(const BookmarkBarPreloadPipelineManager&&) =
      delete;
  BookmarkBarPreloadPipelineManager& operator=(
      const BookmarkBarPreloadPipelineManager&&) = delete;
  BookmarkBarPreloadPipelineManager(const BookmarkBarPreloadPipelineManager&) =
      delete;
  BookmarkBarPreloadPipelineManager& operator=(
      const BookmarkBarPreloadPipelineManager&) = delete;

  static BookmarkBarPreloadPipelineManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // TODO(crbug.com/413259638): Instead of exporting
  // StartPrefetch/StartPrerender directly to the embedder triggers, introduce
  // another function taking the signal (MousePress/MouseHover) and the level of
  // the signal to determine the proper preloading action.
  // The ordering of prefetch and prerender can also be guaranteed by the
  // design.
  void StartPrefetch(const GURL& url);
  void StartPrerender(const GURL& url);

  bool IsPreloadingStarted() { return pipeline_ != nullptr; }
  bool IsPrerenderValidForTesting() {
    return pipeline_ && pipeline_->IsPrerenderValid();
  }

  void SetOnPrefetchCompletedOrFailedCallbackForTesting(
      base::RepeatingCallback<
          void(const network::URLLoaderCompletionStatus& completion_status,
               const std::optional<int>& response_code)>
          on_prefetch_completed_or_failed);

  void ResetPrerender();

 private:

  // Resets the pipeline to allow another preloading attempt if a given url is
  // different from the started one. Pipeline creation will follow the check if
  // a pipeline hasn't existed.
  void EnsurePipelineForUrl(const GURL& url);

  base::RepeatingCallback<void(
      const network::URLLoaderCompletionStatus& completion_status,
      const std::optional<int>& response_code)>
      on_prefetch_completed_or_failed_for_testing_;

  std::unique_ptr<BookmarkBarPreloadPipeline> pipeline_;
};

#endif  // CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_
