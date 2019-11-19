// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "components/download/public/common/all_download_event_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"

using DownloadItem = download::DownloadItem;
using SimpleDownloadManagerCoordinator =
    download::SimpleDownloadManagerCoordinator;
using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    offline_items_collection::OfflineContentAggregator;
using UpdateDelta = offline_items_collection::UpdateDelta;
using LaunchLocation = offline_items_collection::LaunchLocation;

class Profile;
class SkBitmap;

// This class handles the task of observing the downloads associated with a
// SimpleDownloadManagerCoordinator and notifies UI about updates about various
// downloads. This is a per-profile class which works with both reduced mode and
// full browser mode. It also provides internal buffering of the download
// actions if the required backend is not ready.
class DownloadOfflineContentProvider
    : public KeyedService,
      public OfflineContentProvider,
      public DownloadItem::Observer,
      public SimpleDownloadManagerCoordinator::Observer {
 public:
  explicit DownloadOfflineContentProvider(OfflineContentAggregator* aggregator,
                                          const std::string& name_space);
  ~DownloadOfflineContentProvider() override;

  // Should be called when a DownloadManager is available.
  void SetSimpleDownloadManagerCoordinator(
      SimpleDownloadManagerCoordinator* manager);

  // OfflineContentProvider implmentation.

  // Some of these methods can be run in reduced mode while others require the
  // full browser process to be started as mentioned below.

  // Methods that require full browser process.
  void OpenItem(LaunchLocation location, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void GetItemById(
      const ContentId& id,
      OfflineContentProvider::SingleItemCallback callback) override;
  void GetAllItems(
      OfflineContentProvider::MultipleItemCallback callback) override;
  void GetVisualsForItem(
      const ContentId& id,
      GetVisualsOptions options,
      OfflineContentProvider::VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;

  // Methods that can be run in reduced mode.
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

  // Entry point for associating this class with a download item. Must be called
  // for all new and in-progress downloads, after which this class will start
  // observing the given download.
  void OnDownloadStarted(DownloadItem* download_item);

  // DownloadItem::Observer overrides
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadRemoved(DownloadItem* item) override;

  void OnProfileCreated(Profile* profile);

 private:
  enum class State {
    // Download system is not yet initialized.
    UNINITIALIZED,

    // Only active downloads have been loaded.
    ACTIVE_DOWNLOADS_ONLY,

    // All downloads including ones from history have been loaded.
    HISTORY_LOADED,
  };

  // SimpleDownloadManagerCoordinator::Observer overrides
  void OnDownloadsInitialized(bool active_downloads_only) override;
  void OnManagerGoingDown(SimpleDownloadManagerCoordinator* manager) override;

  void GetAllDownloads(std::vector<DownloadItem*>* all_items);
  DownloadItem* GetDownload(const std::string& download_guid);
  void OnThumbnailRetrieved(const ContentId& id,
                            VisualsCallback callback,
                            const SkBitmap& bitmap);
  void AddCompletedDownload(DownloadItem* item);
  void AddCompletedDownloadDone(const std::string& download_guid,
                                int64_t system_download_id);
  void OnRenameDownloadCallbackDone(RenameCallback callback,
                                    DownloadItem* item,
                                    DownloadItem::DownloadRenameResult result);
  void UpdateObservers(const OfflineItem& item,
                       const base::Optional<UpdateDelta>& update_delta);
  void CheckForExternallyRemovedDownloads();

  // Ensure that download core service is started.
  void EnsureDownloadCoreServiceStarted();

  base::ObserverList<OfflineContentProvider::Observer>::Unchecked observers_;
  OfflineContentAggregator* aggregator_;
  std::string name_space_;
  SimpleDownloadManagerCoordinator* manager_;

  std::unique_ptr<download::AllDownloadEventNotifier::Observer>
      all_download_observer_;
  bool checked_for_externally_removed_downloads_;
  State state_;
  base::circular_deque<base::OnceClosure> pending_actions_for_reduced_mode_;
  base::circular_deque<base::OnceClosure> pending_actions_for_full_browser_;

  Profile* profile_;

  base::WeakPtrFactory<DownloadOfflineContentProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadOfflineContentProvider);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
