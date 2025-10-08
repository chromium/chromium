// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"

namespace page_content_annotations {
class PageContentAnnotationsResult;
}

// AuxiliarySearchDonationService manages donation of Chrome data to AppSearch.
// Currently only donates browsing history data.
class AuxiliarySearchDonationService
    : public KeyedService,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit AuxiliarySearchDonationService(
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service);
  ~AuxiliarySearchDonationService() override;

  // page_content_annotations
  //     ::PageContentAnnotationsService
  //     ::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

 private:
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
