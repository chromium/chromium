// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

namespace history {
class HistoryService;
}

namespace subresource_filter {
class SubresourceFilterContentSettingsManager;
}

// Class that observes user changes to history via the HistoryService and
// updates subresource filter-related content settings appropriately.
class SubresourceFilterHistoryObserver
    : public history::HistoryServiceObserver,
      public subresource_filter::SubresourceFilterProfileContext::EmbedderData {
 public:
  // Both |settings_manager| and |history_service| should be non-null.
  SubresourceFilterHistoryObserver(
      subresource_filter::SubresourceFilterContentSettingsManager*
          settings_manager,
      history::HistoryService* history_service);
  ~SubresourceFilterHistoryObserver() override;
  SubresourceFilterHistoryObserver(const SubresourceFilterHistoryObserver&) =
      delete;
  SubresourceFilterHistoryObserver& operator=(
      const SubresourceFilterHistoryObserver&) = delete;

 private:
  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};

  // Outlives this object.
  raw_ptr<subresource_filter::SubresourceFilterContentSettingsManager,
          AcrossTasksDanglingUntriaged>
      settings_manager_;
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_
