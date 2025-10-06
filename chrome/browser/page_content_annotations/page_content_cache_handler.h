// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_CACHE_HANDLER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_CACHE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/scoped_observation.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/visibility.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace page_content_annotations {

class PageContentCache;

// Handles notifications from various observers to interact with the
// PageContentCache.
class PageContentCacheHandler {
 public:
  PageContentCacheHandler(os_crypt_async::OSCryptAsync* os_crypt_async,
                          const base::FilePath& profile_path);
  ~PageContentCacheHandler();

  PageContentCacheHandler(const PageContentCacheHandler&) = delete;
  PageContentCacheHandler& operator=(const PageContentCacheHandler&) = delete;

  // Called when a tab is closed.
  void OnTabClosed(int64_t tab_id);

  // Called when the visibility of a WebContents changes.
  void OnVisibilityChanged(
      std::optional<int64_t> tab_id,
      content::WebContents* web_contents,
      content::Visibility visibility,
      std::optional<optimization_guide::proto::AnnotatedPageContent> result);

  // Called when a new navigation happens in a WebContents.
  void OnNewNavigation(std::optional<int64_t> tab_id,
                       content::WebContents* web_contents);

  void ProcessPageContentExtraction(
      std::optional<int64_t> tab_id,
      content::WebContents* web_contents,
      const optimization_guide::proto::AnnotatedPageContent& page_content);

  PageContentCache* page_content_cache() { return page_content_cache_.get(); }

 private:
  std::unique_ptr<PageContentCache> page_content_cache_;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_CACHE_HANDLER_H_
