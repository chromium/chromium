// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/visibility.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace page_content_annotations {

struct ExtractedPageContentResult;
class PageContentCache;
class PageContentCacheHandler;

class PageContentExtractionService : public KeyedService,
                                     public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when `page_content` is extracted for `page`. The extraction is
    // triggered for every page once the page has sufficiently loaded.
    virtual void OnPageContentExtracted(
        content::Page& page,
        const optimization_guide::proto::AnnotatedPageContent& page_content) {}
  };

  PageContentExtractionService(os_crypt_async::OSCryptAsync* os_crypt_async,
                               const base::FilePath& profile_path);
  ~PageContentExtractionService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether page content extraction should be enabled. It should be
  // enabled based on features, or when some observer has registered for page
  // content.
  bool ShouldEnablePageContentExtraction() const;

  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available.
  std::optional<ExtractedPageContentResult>
  GetExtractedPageContentAndEligibilityForPage(content::Page& page);

  // Called when a tab is closed.
  void OnTabClosed(int64_t tab_id);

  // Called when the visibility of a WebContents changes.
  void OnVisibilityChanged(std::optional<int64_t> tab_id,
                           content::WebContents* web_contents,
                           content::Visibility visibility);

  // Called when a new navigation happens in a WebContents.
  void OnNewNavigation(std::optional<int64_t> tab_id,
                       content::WebContents* web_contents);

  // Disk cache for getting page contents for tabs without webcontents.
  PageContentCache* GetPageContentCache();

 protected:
  friend class AnnotatedPageContentRequest;

  // Invoked when `page_content` is extracted for `page`, to notify the
  // observers. `tab_id` for the tab where page is loaded, if available.
  virtual void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content,
      std::optional<int> tab_id);

  std::optional<ExtractedPageContentResult> GetCachedContentsFromWebContents(
      content::WebContents* web_contents);

  base::ObserverList<Observer> observers_;

  const bool is_page_content_cache_enabled_;
  const std::unique_ptr<PageContentCacheHandler> page_content_cache_handler_;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_
