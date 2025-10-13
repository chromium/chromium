// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"

namespace page_content_annotations {
class PageContentAnnotationsResult;
}
namespace visited_url_ranking {
class VisitedURLRankingService;
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
          page_content_annotations_service,
      visited_url_ranking::VisitedURLRankingService* ranking_service);
  ~AuxiliarySearchDonationService() override;

  // page_content_annotations
  //     ::PageContentAnnotationsService
  //     ::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

  // Returns the delay between a new content annotation and when a history
  // donation is triggered.
  base::TimeDelta GetDonationDelayForTesting() const;

 private:
  void FetchHistoryAndDonate();
  void DonateHistoryEntries(
      std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries);

  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  raw_ptr<visited_url_ranking::VisitedURLRankingService> ranking_service_;
  base::OneShotTimer donation_timer_;
  base::WeakPtrFactory<AuxiliarySearchDonationService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
