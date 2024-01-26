// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

class Profile;

using ContentId = offline_items_collection::ContentId;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    offline_items_collection::OfflineContentAggregator;
using OfflineItem = offline_items_collection::OfflineItem;
using UpdateDelta = offline_items_collection::UpdateDelta;

// Class for notifying UI when an OfflineItem should be displayed.
class DownloadShelfController : public OfflineContentProvider::Observer {
 public:
  explicit DownloadShelfController(Profile* profile);

  DownloadShelfController(const DownloadShelfController&) = delete;
  DownloadShelfController& operator=(const DownloadShelfController&) = delete;

  ~DownloadShelfController() override;

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  // Called when a new OfflineItem is to be displayed on UI.
  void OnNewOfflineItemReady(DownloadUIModel::DownloadUIModelPtr model);

  raw_ptr<Profile> profile_;
  raw_ptr<OfflineContentAggregator> aggregator_;
  base::ScopedObservation<OfflineContentProvider,
                          OfflineContentProvider::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTROLLER_H_
