// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// BrowsingDataHistoryObserverService is listening for history deletions to
// remove navigation, session and recent tab entries.
class BrowsingDataHistoryObserverService
    : public KeyedService,
      public history::HistoryServiceObserver {
 public:
  explicit BrowsingDataHistoryObserverService(Profile* profile);

  BrowsingDataHistoryObserverService(
      const BrowsingDataHistoryObserverService&) = delete;
  BrowsingDataHistoryObserverService& operator=(
      const BrowsingDataHistoryObserverService&) = delete;

  ~BrowsingDataHistoryObserverService() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  class Factory : public ProfileKeyedServiceFactory {
   public:
    static Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override = default;

    // BrowserContextKeyedServiceFactory:
    std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
  };

 private:
  raw_ptr<Profile> profile_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_
