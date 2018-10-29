// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

using DownloadItem = download::DownloadItem;
using DownloadManager = content::DownloadManager;
using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    offline_items_collection::OfflineContentAggregator;
using LaunchLocation = offline_items_collection::LaunchLocation;

class SkBitmap;

// This class handles the task of observing a single DownloadManager and
// notifies UI about updates about various downloads.
class DownloadOfflineContentProvider
    : public OfflineContentProvider,
      public download::AllDownloadItemNotifier::Observer {
 public:
  explicit DownloadOfflineContentProvider(DownloadManager* manager);
  ~DownloadOfflineContentProvider() override;

  // OfflineContentProvider implmentation.
  void OpenItem(LaunchLocation location, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;
  void GetItemById(
      const ContentId& id,
      OfflineContentProvider::SingleItemCallback callback) override;
  void GetAllItems(
      OfflineContentProvider::MultipleItemCallback callback) override;
  void GetVisualsForItem(
      const ContentId& id,
      OfflineContentProvider::VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback callback) override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

 private:
  // AllDownloadItemNotifier::Observer methods.
  void OnDownloadUpdated(DownloadManager* manager, DownloadItem* item) override;
  void OnDownloadRemoved(DownloadManager* manager, DownloadItem* item) override;

  void OnThumbnailRetrieved(const ContentId& id,
                            VisualsCallback callback,
                            const SkBitmap& bitmap);

  DownloadManager* manager_;
  download::AllDownloadItemNotifier download_notifier_;
  base::ObserverList<OfflineContentProvider::Observer>::Unchecked observers_;
  OfflineContentAggregator* aggregator_;
  std::string name_space_;

  base::WeakPtrFactory<DownloadOfflineContentProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadOfflineContentProvider);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
