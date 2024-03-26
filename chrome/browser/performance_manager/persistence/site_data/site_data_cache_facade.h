// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace performance_manager {

// This class serves as an interface between a SiteDataCache living on the PM
// sequence and the UI thread. This is meant to be used by a
// BrowserContextKeyedServiceFactory to manage the lifetime of this cache.
//
// Instances of this class are expected to live on the UI thread.
class SiteDataCacheFacade : public KeyedService,
                            public history::HistoryServiceObserver {
 public:
  explicit SiteDataCacheFacade(content::BrowserContext* browser_context);

  SiteDataCacheFacade(const SiteDataCacheFacade&) = delete;
  SiteDataCacheFacade& operator=(const SiteDataCacheFacade&) = delete;

  ~SiteDataCacheFacade() override;

  void IsDataCacheRecordingForTesting(base::OnceCallback<void(bool)> cb);

  void WaitUntilCacheInitializedForTesting();

  void ClearAllSiteDataForTesting() { ClearAllSiteData(); }

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  // Implementation of OnURLsDeleted(), in a separate method so it can be called
  // directly by ClearAllSiteDataForTesting().
  void ClearAllSiteData();

  // The browser context associated with this cache.
  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_H_
