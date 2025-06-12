// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_
#define CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_

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

  // Returns true if prerender starts successfully or a started prerender is
  // present, false otherwise.
  bool StartPrerender(content::WebContents& web_contents,
                      content::PreloadingPredictor predictor);

  const GURL& url() const { return url_; }

 private:
  const scoped_refptr<content::PreloadPipelineInfo> pipeline_info_;

  const GURL url_;

  std::unique_ptr<content::PrerenderHandle> prerender_handle_;
};

#endif  // CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_H_
