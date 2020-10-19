// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_

#include "base/scoped_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

class SubresourceFilterContentSettingsManager;

namespace history {
class HistoryService;
}

// Class that observes user changes to history via the HistoryService and
// updates subresource filter-related content settings appropriately.
class SubresourceFilterHistoryObserver
    : public history::HistoryServiceObserver,
      public SubresourceFilterProfileContext::EmbedderData {
 public:
  // Both |settings_manager| and |history_service| should be non-null.
  SubresourceFilterHistoryObserver(
      SubresourceFilterContentSettingsManager* settings_manager,
      history::HistoryService* history_service);
  ~SubresourceFilterHistoryObserver() override;
  SubresourceFilterHistoryObserver(const SubresourceFilterHistoryObserver&) =
      delete;
  SubresourceFilterHistoryObserver& operator=(
      const SubresourceFilterHistoryObserver&) = delete;

 private:
  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};

  // Outlives this object.
  SubresourceFilterContentSettingsManager* settings_manager_;
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_HISTORY_OBSERVER_H_
