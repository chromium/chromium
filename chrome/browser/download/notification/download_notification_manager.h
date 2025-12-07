// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/notification/download_item_notification.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"

class Profile;

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::UpdateDelta;

class DownloadNotificationManager : public DownloadUIController::Delegate,
                                    public DownloadItemNotification::Observer,
                                    public OfflineContentProvider::Observer {
 public:
  explicit DownloadNotificationManager(Profile* profile);

  DownloadNotificationManager(const DownloadNotificationManager&) = delete;
  DownloadNotificationManager& operator=(const DownloadNotificationManager&) =
      delete;

  ~DownloadNotificationManager() override;

  // DownloadUIController::Delegate overrides.
  void OnNewDownloadReady(download::DownloadItem* item) override;

  // DownloadItemNotification::Observer overrides.
  void OnDownloadDestroyed(const ContentId& contentId) override;

 private:
  friend class test::DownloadItemNotificationTest;

  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  void PrioritizeDownloadItemNotification(
      DownloadUIModel::DownloadUIModelPtr ui_model);

  raw_ptr<Profile> profile_;
  std::map<ContentId, std::unique_ptr<DownloadItemNotification>> items_;
  raw_ptr<OfflineContentAggregator> aggregator_;
  base::ScopedObservation<OfflineContentProvider,
                          OfflineContentProvider::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_NOTIFICATION_MANAGER_H_
