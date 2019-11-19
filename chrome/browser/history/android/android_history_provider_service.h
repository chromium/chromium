// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/history/core/browser/android/android_history_types.h"

class Profile;

// This class provides the methods to communicate with history backend service
// for the Android content provider.
// The methods of this class must run on the UI thread to cooperate with the
// BookmarkModel task posted in the DB thread.
class AndroidHistoryProviderService {
 public:
  explicit AndroidHistoryProviderService(Profile* profile);
  virtual ~AndroidHistoryProviderService();

  // The callback definitions ------------------------------------------------

  // Callback invoked when a method creating an |AndroidStatement| object is
  // complete. The pointer is NULL if the creation failed.
  typedef base::Callback<void(history::AndroidStatement*)> QueryCallback;

  // Callback invoked when a method updating rows in the database complete.
  // The parameter is the number of rows updated or 0 if the update failed.
  typedef base::Callback<void(int)> UpdateCallback;

  // Callback invoked when a method inserting rows in the database complete.
  // The value is the new row id or 0 if the insertion failed.
  typedef base::Callback<void(int64_t)> InsertCallback;

  // Callback invoked when a method deleting rows in the database complete.
  // The value is the number of rows deleted or 0 if the deletion failed.
  typedef base::Callback<void(int)> DeleteCallback;

  // Callback invoked when a method moving an |AndroidStatement| is complete.
  // The value passed to the callback is the new position, or in case of
  // failure, the old position.
  typedef base::Callback<void(int)> MoveStatementCallback;

  // History and Bookmarks ----------------------------------------------------
  //
  // Runs the given query on history backend, and invokes the |callback| to
  // return the result.
  //
  // |projections| is the vector of the result columns.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| is the SQL ORDER clause.
  base::CancelableTaskTracker::TaskId QueryHistoryAndBookmarks(
      const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order,
      const QueryCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Runs the given update and the number of the row updated is returned to the
  // |callback| on success.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  base::CancelableTaskTracker::TaskId UpdateHistoryAndBookmarks(
      const history::HistoryAndBookmarkRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const UpdateCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Deletes the specified rows and invokes the |callback| to return the number
  // of row deleted on success.
  //
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // If |selection| is empty all history and bookmarks are deleted.
  base::CancelableTaskTracker::TaskId DeleteHistoryAndBookmarks(
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const DeleteCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Inserts the given values into history backend, and invokes the |callback|
  // to return the result.
  base::CancelableTaskTracker::TaskId InsertHistoryAndBookmark(
      const history::HistoryAndBookmarkRow& values,
      const InsertCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Deletes the matched history and invokes |callback| to return the number of
  // rows deleted.
  base::CancelableTaskTracker::TaskId DeleteHistory(
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const DeleteCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Statement ----------------------------------------------------------------
  // Moves the statement's current row from |current_pos| to |destination| in DB
  // thread. The new position is returned to the callback. The result supplied
  // the callback is constrained by the number of rows might.
  base::CancelableTaskTracker::TaskId MoveStatement(
      history::AndroidStatement* statement,
      int current_pos,
      int destination,
      const MoveStatementCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Closes the statement in db thread. The AndroidHistoryProviderService takes
  // the ownership of |statement|.
  void CloseStatement(history::AndroidStatement* statement);

  // Search term --------------------------------------------------------------
  // Inserts the given values and returns the SearchTermID of the inserted row
  // to the |callback| on success.
  base::CancelableTaskTracker::TaskId InsertSearchTerm(
      const history::SearchRow& row,
      const InsertCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Runs the given update and returns the number of the update rows to the
  // |callback| on success.
  //
  // |row| is the value need to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  base::CancelableTaskTracker::TaskId UpdateSearchTerms(
      const history::SearchRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const UpdateCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Deletes the matched rows and the number of deleted rows is returned to
  // the |callback| on success.
  //
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  //
  // If |selection| is empty all search terms will be deleted.
  base::CancelableTaskTracker::TaskId DeleteSearchTerms(
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const DeleteCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Runs the query and invokes the |callback| to return the result.
  //
  // |projections| specifies the result columns, can not be empty, otherwise
  // NULL is returned.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| the SQL ORDER clause.
  base::CancelableTaskTracker::TaskId QuerySearchTerms(
      const std::vector<history::SearchRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order,
      const QueryCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Returns the largest Favicon for |favicon_id| and invokes
  // the |callback| to return the result.
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForID(
      favicon_base::FaviconID favicon_id,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AndroidHistoryProviderService);
};

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_
