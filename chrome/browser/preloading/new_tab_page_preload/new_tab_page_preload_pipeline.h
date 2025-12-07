// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_H_
#define CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_H_

#include "content/public/browser/prefetch_handle.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/prerender_handle.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Represents a pipeline for NewTabPage preloads.
class NewTabPagePreloadPipeline {
 public:
  explicit NewTabPagePreloadPipeline(GURL url);
  ~NewTabPagePreloadPipeline();

  // Not movable nor copyable.
  NewTabPagePreloadPipeline(const NewTabPagePreloadPipeline&&) = delete;
  NewTabPagePreloadPipeline& operator=(const NewTabPagePreloadPipeline&&) =
      delete;
  NewTabPagePreloadPipeline(const NewTabPagePreloadPipeline&) = delete;
  NewTabPagePreloadPipeline& operator=(const NewTabPagePreloadPipeline&) =
      delete;

  void StartPrefetch(content::WebContents& web_contents,
                     content::PreloadingPredictor predictor);

  void StartPrerender(content::WebContents& web_contents,
                      content::PreloadingPredictor predictor);

  const GURL& url() const { return url_; }

 private:
  const scoped_refptr<content::PreloadPipelineInfo> pipeline_info_;

  const GURL url_;

  std::unique_ptr<content::PrefetchHandle> prefetch_handle_;

  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
};

#endif  // CHROME_BROWSER_PRELOADING_NEW_TAB_PAGE_PRELOAD_NEW_TAB_PAGE_PRELOAD_PIPELINE_H_
