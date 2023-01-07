// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/download_manager.h"

namespace history {
struct DownloadRow;
}  // namespace history

// Observes a single DownloadManager and all its DownloadItems, keeping the
// DownloadDatabase up to date.
class DownloadHistory : public download::AllDownloadItemNotifier::Observer {
 public:
  typedef std::set<uint32_t> IdSet;

  // Caller must guarantee that HistoryService outlives HistoryAdapter.
  class HistoryAdapter {
   public:
    explicit HistoryAdapter(history::HistoryService* history);

    HistoryAdapter(const HistoryAdapter&) = delete;
    HistoryAdapter& operator=(const HistoryAdapter&) = delete;

    virtual ~HistoryAdapter();

    virtual void QueryDownloads(
        history::HistoryService::DownloadQueryCallback callback);

    virtual void CreateDownload(
        const history::DownloadRow& info,
        history::HistoryService::DownloadCreateCallback callback);

    virtual void UpdateDownload(const history::DownloadRow& data,
                                bool should_commit_immediately);

    virtual void RemoveDownloads(const std::set<uint32_t>& ids);

   private:
    raw_ptr<history::HistoryService> history_;
  };

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Fires when a download is added to or updated in the database, just after
    // the task is posted to the history thread.
    virtual void OnDownloadStored(download::DownloadItem* item,
                                  const history::DownloadRow& info) {}

    // Fires when RemoveDownloads messages are sent to the DB thread.
    virtual void OnDownloadsRemoved(const IdSet& ids) {}

    // Fires when the DownloadHistory completes the initial history query.
    // Unlike the other observer methods, this one is invoked if the initial
    // history query has already completed by the time the caller calls
    // AddObserver().
    virtual void OnHistoryQueryComplete() {}

    // Fires when the DownloadHistory is being destroyed so that implementors
    // can RemoveObserver() and nullify their DownloadHistory*s.
    virtual void OnDownloadHistoryDestroyed() {}
  };

  // Returns true if the download is persisted. Not reliable when called from
  // within a DownloadManager::Observer::OnDownloadCreated handler since the
  // persisted state may not yet have been updated for a download that was
  // restored from history.
  static bool IsPersisted(const download::DownloadItem* item);

  // Neither |manager| nor |history| may be NULL.
  // DownloadService creates DownloadHistory some time after DownloadManager is
  // created and destroys DownloadHistory as DownloadManager is shutting down.
  DownloadHistory(content::DownloadManager* manager,
                  std::unique_ptr<HistoryAdapter> history);

  DownloadHistory(const DownloadHistory&) = delete;
  DownloadHistory& operator=(const DownloadHistory&) = delete;

  ~DownloadHistory() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Callback from |history_| containing all entries in the downloads database
  // table.
  void QueryCallback(std::vector<history::DownloadRow> rows);

  // Called to create all history downloads.
  void LoadHistoryDownloads(const std::vector<history::DownloadRow>& rows);

  // May add |item| to |history_|.
  void MaybeAddToHistory(download::DownloadItem* item);

  // Callback from |history_| when an item was successfully inserted into the
  // database.
  void ItemAdded(uint32_t id, const history::DownloadRow& info, bool success);

  // AllDownloadItemNotifier::Observer
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadOpened(content::DownloadManager* manager,
                        download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // Schedule a record to be removed from |history_| the next time
  // RemoveDownloadsBatch() runs. Schedule RemoveDownloadsBatch() to be run soon
  // if it isn't already scheduled.
  void ScheduleRemoveDownload(uint32_t download_id);

  // Removes all |removing_ids_| from |history_|.
  void RemoveDownloadsBatch();

  // Called when a download was restored from history.
  void OnDownloadRestoredFromHistory(download::DownloadItem* item);

  // Check whether an download item needs be updated or added to history DB.
  bool NeedToUpdateDownloadHistory(download::DownloadItem* item);

  download::AllDownloadItemNotifier notifier_;

  std::unique_ptr<HistoryAdapter> history_;

  // Identifier of the item being created in QueryCallback(), matched up with
  // created items in OnDownloadCreated() so that the item is not re-added to
  // the database.
  uint32_t loading_id_;

  // Identifiers of items that are scheduled for removal from history, to
  // facilitate batching removals together for database efficiency.
  IdSet removing_ids_;

  // |GetId()|s of items that were removed while they were being added, so that
  // they can be removed when the database finishes adding them.
  // TODO(benjhayden) Can this be removed now that it doesn't need to wait for
  // the db_handle, and can rely on PostTask sequentiality?
  IdSet removed_while_adding_;

  bool initial_history_query_complete_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<DownloadHistory> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
