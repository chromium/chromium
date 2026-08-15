// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/prefs/pref_member.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "url/gurl.h"

class PrefService;
class PrefRegistrySimple;
namespace page_content_annotations {
class PageContentAnnotationsResult;
}
struct CoreAccountInfo;
namespace signin {
class IdentityManager;
}
namespace visited_url_ranking {
class VisitedURLRankingService;
struct URLVisitsMetadata;
struct URLVisitAggregate;
}

// AuxiliarySearchDonationService manages donation of Chrome data to AppSearch.
// Currently only donates browsing history data.
// The provided donate callback is only called with non-empty vectors.
class AuxiliarySearchDonationService
    : public KeyedService,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  // Data for a single history entry.
  struct HistoryData {
    // From `URLVisitAggregate`.
    std::string url_key;

    // From `URLVisitAggregate::HistoryData::visit`.
    GURL url;
    std::u16string title;

    // From `URLVisitAggregate::HistoryData::last_visited::visit_row`.
    base::Time last_visited;

    HistoryData(std::string url_key,
                GURL url,
                std::u16string title,
                base::Time last_visited);

    HistoryData(const HistoryData&);
    HistoryData& operator=(const HistoryData&);
    HistoryData(HistoryData&&);
    HistoryData& operator=(HistoryData&&);

    ~HistoryData();
  };
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void DonateHistoryEntries(std::vector<HistoryData> entries,
                                      CoreAccountInfo account_info) = 0;
    virtual void SetBrowsingDataDonationEnabled(bool enabled) = 0;
  };

  explicit AuxiliarySearchDonationService(
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      visited_url_ranking::VisitedURLRankingService* ranking_service,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      std::unique_ptr<Delegate> testing_delegate = nullptr);
  ~AuxiliarySearchDonationService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // page_content_annotations
  //     ::PageContentAnnotationsService
  //     ::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

  // Returns the delay between a new content annotation and when a history
  // donation is triggered.
  base::TimeDelta GetDonationDelay() const;

  // Returns the maximum duration before "now" to fetch history from.
  base::TimeDelta GetHistoryAgeThresholdForTesting() const;

 private:
  void FetchHistoryAndDonate();
  void OnHistoryFetched(
      visited_url_ranking::ResultStatus status,
      visited_url_ranking::URLVisitsMetadata url_visits_metadata,
      std::vector<visited_url_ranking::URLVisitAggregate> aggregates);
  void DonateHistoryEntries(
      std::vector<HistoryData> entries,
      const visited_url_ranking::URLVisitsMetadata& metadata);
  void OnApplicationStateChanged(base::android::ApplicationState state);
  void OnBrowsingDataDonationPrefChanged();

  const raw_ref<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  const raw_ref<visited_url_ranking::VisitedURLRankingService> ranking_service_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<PrefService> pref_service_;
  std::unique_ptr<Delegate> delegate_;
  BooleanPrefMember is_browsing_data_donation_enabled_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
  base::OneShotTimer donation_timer_;
  base::WeakPtrFactory<AuxiliarySearchDonationService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_H_
