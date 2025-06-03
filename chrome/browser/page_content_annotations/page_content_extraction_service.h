// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace content {
class Page;
}

namespace page_content_annotations {

class PageContentExtractionService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when `page_content` is extracted for `page`. The extraction is
    // triggered for every page once the page has sufficiently loaded.
    virtual void OnPageContentExtracted(
        content::Page& page,
        const optimization_guide::proto::AnnotatedPageContent& page_content) {}
  };

  PageContentExtractionService();
  ~PageContentExtractionService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether page content extraction should be enabled. It should be
  // enabled based on features, or when some observer has registered for page
  // content.
  bool ShouldEnablePageContentExtraction() const;

 private:
  friend class AnnotatedPageContentRequest;

  // Invoked when `page_content` is extracted for `page`, to notify the
  // observers.
  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent& page_content);

  base::ObserverList<Observer> observers_;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_SERVICE_H_
