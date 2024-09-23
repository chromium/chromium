// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_HISTORY_AWARE_SITE_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENGAGEMENT_HISTORY_AWARE_SITE_ENGAGEMENT_SERVICE_H_

#include <set>

#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/site_engagement/content/site_engagement_service.h"

namespace content {
class BrowserContext;
}

namespace site_engagement {

// A version of SiteEngagementService that observes changes to (deletions of)
// history.
class HistoryAwareSiteEngagementService
    : public SiteEngagementService,
      public history::HistoryServiceObserver {
 public:
  HistoryAwareSiteEngagementService(content::BrowserContext* browser_context,
                                    history::HistoryService* history_service);
  HistoryAwareSiteEngagementService(
      const HistoryAwareSiteEngagementService& other) = delete;
  HistoryAwareSiteEngagementService& operator=(
      const HistoryAwareSiteEngagementService& other) = delete;
  ~HistoryAwareSiteEngagementService() override;

  // SiteEngagementService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  // Updates site engagement scores after some data has been deleted.
  void UpdateEngagementScores(
      const std::multiset<GURL>& deleted_url_origins,
      bool expired,
      const history::OriginCountAndLastVisitMap& remaining_origin_counts);

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

}  // namespace site_engagement

#endif  // CHROME_BROWSER_ENGAGEMENT_HISTORY_AWARE_SITE_ENGAGEMENT_SERVICE_H_
