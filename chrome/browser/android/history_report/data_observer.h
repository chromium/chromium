// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"

namespace history {

class HistoryService;
}

namespace history_report {

class DeltaFileService;
class UsageReportsBufferService;

// Observes history data for changes and updates delta file accordingly.
class DataObserver : public bookmarks::BookmarkModelObserver,
                     public history::HistoryServiceObserver {
 public:
  DataObserver(base::RepeatingCallback<void(void)> data_changed_callback,
               base::RepeatingCallback<void(void)> data_cleared_callback,
               base::RepeatingCallback<void(void)> stop_reporting_callback,
               DeltaFileService* delta_file_service,
               UsageReportsBufferService* usage_reports_buffer_service,
               bookmarks::BookmarkModel* bookmark_model,
               history::HistoryService* history_service);

  DataObserver(const DataObserver&) = delete;
  DataObserver& operator=(const DataObserver&) = delete;

  ~DataObserver() override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(
      bookmarks::BookmarkModel* model,
      const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;

  // HistoryServiceObserver implementation.
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  void DeleteBookmarks(const std::set<GURL>& removed_urls);

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_bookmark_model_observer_{this};
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};
  base::RepeatingCallback<void(void)> data_changed_callback_;
  base::RepeatingCallback<void(void)> data_cleared_callback_;
  base::RepeatingCallback<void(void)> stop_reporting_callback_;
  raw_ptr<DeltaFileService> delta_file_service_;
  raw_ptr<UsageReportsBufferService> usage_reports_buffer_service_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_
