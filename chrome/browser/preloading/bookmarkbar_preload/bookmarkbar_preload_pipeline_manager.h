// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/bookmarkbar_preload/bookmarkbar_preload_pipeline.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Roles:
//
// - Manages preload pipelines per WebContents.
class BookmarkBarPreloadPipelineManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkBarPreloadPipelineManager> {
 public:
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

  base::WeakPtr<BookmarkBarPreloadPipelineManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void StartPrerender(const GURL& url);

  bool IsPreloadingStarted() { return pipeline_ != nullptr; }

  void ResetPrerender();

 private:
  friend content::WebContentsUserData<BookmarkBarPreloadPipelineManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit BookmarkBarPreloadPipelineManager(content::WebContents* contents);

  std::unique_ptr<BookmarkBarPreloadPipeline> pipeline_;
  base::WeakPtrFactory<BookmarkBarPreloadPipelineManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_BOOKMARKBAR_PRELOAD_BOOKMARKBAR_PRELOAD_PIPELINE_MANAGER_H_
