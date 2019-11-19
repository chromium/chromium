// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_

#include "base/scoped_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// BrowsingDataHistoryObserverService is listening for history deletions to
// remove navigation, session and recent tab entries.
class BrowsingDataHistoryObserverService
    : public KeyedService,
      public history::HistoryServiceObserver {
 public:
  explicit BrowsingDataHistoryObserverService(Profile* profile);
  ~BrowsingDataHistoryObserverService() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override = default;

    // BrowserContextKeyedServiceFactory:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    bool ServiceIsCreatedWithBrowserContext() const override;
  };

 private:
  Profile* profile_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHistoryObserverService);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HISTORY_OBSERVER_SERVICE_H_
