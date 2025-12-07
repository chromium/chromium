// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_
#define CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_

#include "content/public/browser/prefetch_handle.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/prerender_handle.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Represents a pipeline for BookmarkBar preloads.
class BookmarkBarPreloadPipeline {
 public:
  explicit BookmarkBarPreloadPipeline(GURL url);
  ~BookmarkBarPreloadPipeline();

  // Not movable nor copyable.
  BookmarkBarPreloadPipeline(const BookmarkBarPreloadPipeline&&) = delete;
  BookmarkBarPreloadPipeline& operator=(const BookmarkBarPreloadPipeline&&) =
      delete;
  BookmarkBarPreloadPipeline(const BookmarkBarPreloadPipeline&) = delete;
  BookmarkBarPreloadPipeline& operator=(const BookmarkBarPreloadPipeline&) =
      delete;

  void StartPrefetch(content::WebContents& web_contents,
                     content::PreloadingPredictor predictor);

  void StartPrerender(content::WebContents& web_contents,
                      content::PreloadingPredictor predictor);
  bool IsPrerenderValid() {
    return prerender_handle_ && prerender_handle_->IsValid();
  }
  void ResetPrerender() { prerender_handle_.reset(); }

  const GURL& url() const { return url_; }

  void SetOnPrefetchCompletedOrFailedCallbackForTesting(
      base::RepeatingCallback<
          void(const network::URLLoaderCompletionStatus& completion_status,
               const std::optional<int>& response_code)>
          on_prefetch_completed_or_failed);

 private:
  const scoped_refptr<content::PreloadPipelineInfo> pipeline_info_;

  const GURL url_;

  std::unique_ptr<content::PrefetchHandle> prefetch_handle_;
  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
};

#endif  // CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_
