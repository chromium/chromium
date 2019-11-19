// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
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
  DataObserver(base::Callback<void(void)> data_changed_callback,
               base::Callback<void(void)> data_cleared_callback,
               base::Callback<void(void)> stop_reporting_callback,
               DeltaFileService* delta_file_service,
               UsageReportsBufferService* usage_reports_buffer_service,
               bookmarks::BookmarkModel* bookmark_model,
               history::HistoryService* history_service);
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
                         size_t index) override;
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
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

 private:
  void DeleteBookmarks(const std::set<GURL>& removed_urls);

  bookmarks::BookmarkModel* bookmark_model_;
  base::Callback<void(void)> data_changed_callback_;
  base::Callback<void(void)> data_cleared_callback_;
  base::Callback<void(void)> stop_reporting_callback_;
  DeltaFileService* delta_file_service_;
  UsageReportsBufferService* usage_reports_buffer_service_;

  DISALLOW_COPY_AND_ASSIGN(DataObserver);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_DATA_OBSERVER_H_
