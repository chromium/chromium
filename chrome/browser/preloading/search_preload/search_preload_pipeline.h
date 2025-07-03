// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_H_
#define CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_H_

#include "chrome/browser/preloading/search_preload/search_preload_signal_result.h"
#include "content/public/browser/prefetch_handle.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/prerender_handle.h"
#include "url/gurl.h"

class SearchPreloadService;

namespace content {
class WebContents;
}

// Represents a pipeline for Default Search Engine preloads.
class SearchPreloadPipeline {
 public:
  explicit SearchPreloadPipeline(GURL canonical_url);
  ~SearchPreloadPipeline();

  // Not movable nor copyable.
  SearchPreloadPipeline(const SearchPreloadPipeline&&) = delete;
  SearchPreloadPipeline& operator=(const SearchPreloadPipeline&&) = delete;
  SearchPreloadPipeline(const SearchPreloadPipeline&) = delete;
  SearchPreloadPipeline& operator=(const SearchPreloadPipeline&) = delete;

  void UpdateConfidence(content::WebContents& web_contents, int confidence);

  // Starts prefetch if not triggered yet.
  //
  // Returns true iff prefetch is triggered, i.e. `WebContents::StartPrefetch()`
  // is called.
  SearchPreloadSignalResult StartPrefetch(
      content::WebContents& web_contents,
      base::WeakPtr<SearchPreloadService> search_preload_service,
      const GURL& prefetch_url,
      content::PreloadingPredictor predictor,
      const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint,
      bool is_navigation_likely);
  // Starts prerender if not triggered yet and prefetch is alive.
  SearchPreloadSignalResult StartPrerender(
      content::WebContents& web_contents,
      const GURL& prerernder_url,
      content::PreloadingPredictor predictor);

  // Cancels prerender if triggered.
  void CancelPrerender();

  bool IsPrefetchAlive() const;
  bool IsPrerenderValid() const;

 private:
  const scoped_refptr<content::PreloadPipelineInfo> pipeline_info_;

  const GURL canonical_url_;

  int confidence_ = 0;

  std::unique_ptr<content::PrefetchHandle> prefetch_handle_;
  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
};

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_PIPELINE_H_
