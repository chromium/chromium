// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/common/search_result_extractor_client_status.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class TemplateURLService;

namespace prerender {
class NoStatePrefetchManager;
}  // namespace prerender

namespace page_content_annotations {

class PageContentAnnotationsService;

// This class is used to dispatch page content to the
// PageContentAnnotationsService to be annotated.
class PageContentAnnotationsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          PageContentAnnotationsWebContentsObserver> {
 public:
  ~PageContentAnnotationsWebContentsObserver() override;

  PageContentAnnotationsWebContentsObserver(
      const PageContentAnnotationsWebContentsObserver&) = delete;
  PageContentAnnotationsWebContentsObserver& operator=(
      const PageContentAnnotationsWebContentsObserver&) = delete;

 protected:
  explicit PageContentAnnotationsWebContentsObserver(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      PageContentAnnotationsWebContentsObserver>;
  friend class PageContentAnnotationsWebContentsObserverTest;

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Invoked when related searches have been extracted for |visit|.
  void OnRelatedSearchesExtracted(
      const HistoryVisit& visit,
      continuous_search::SearchResultExtractorClientStatus status,
      continuous_search::mojom::CategoryResultsPtr results);

  // Not owned. Guaranteed to outlive |this|.
  raw_ptr<TemplateURLService> template_url_service_;

  // Not owned. Guaranteed to outlive |this|.
  raw_ptr<PageContentAnnotationsService> page_content_annotations_service_;

  // Not owned. Guaranteed to outlive |this|.
  raw_ptr<prerender::NoStatePrefetchManager> no_state_prefetch_manager_;

  // The client of continuous_search::mojom::SearchResultExtractor
  // interface used for extracting data from the main frame of Google SRP
  // |web_contents|.
  continuous_search::SearchResultExtractorClient
      search_result_extractor_client_;

  base::WeakPtrFactory<PageContentAnnotationsWebContentsObserver>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
