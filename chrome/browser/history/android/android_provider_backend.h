// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_

#include <list>
#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "components/history/core/browser/android/android_cache_database.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/android/sql_handler.h"

namespace favicon {
class FaviconDatabase;
}

namespace history {

class AndroidProviderBackend;
class HistoryBackend;
class HistoryBackendClient;
class HistoryBackendNotifier;
class HistoryDatabase;

// This class provides the query/insert/update/remove methods to implement
// android.provider.Browser.BookmarkColumns and
// android.provider.Browser.SearchColumns API.
//
// When used it:
// a. The android_urls table is created in history database if it doesn't
//    exists.
// b. The android_cache database is created.
// c. The bookmark_cache table is created.
//
// Android_urls and android_cache database is only updated before the related
// methods are accessed. A data change will not triger the update.
//
// The android_cache database is deleted when shutdown.
class AndroidProviderBackend : public base::SupportsUserData::Data {
 public:
  AndroidProviderBackend(const base::FilePath& cache_db_name,
                         HistoryDatabase* history_db,
                         favicon::FaviconDatabase* favicon_db,
                         HistoryBackendClient* backend_client,
                         HistoryBackendNotifier* notifier);

  ~AndroidProviderBackend() override;

  static const void* GetUserDataKey();

  static AndroidProviderBackend* FromHistoryBackend(
      HistoryBackend* history_backend);

  // Bookmarks ----------------------------------------------------------------
  //
  // Runs the given query and returns the result on success, NULL on error or
  // the |projections| is empty.
  //
  // |projections| is the vector of the result columns.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| is the SQL ORDER clause.
  AndroidStatement* QueryHistoryAndBookmarks(
      const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order);

  // Runs the given update and returns the number of the updated rows in
  // |update_count| and return true on success, false on error.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  bool UpdateHistoryAndBookmarks(
      const HistoryAndBookmarkRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      int* update_count);

  // Inserts the given values and returns the URLID of the inserted row.
  AndroidURLID InsertHistoryAndBookmark(const HistoryAndBookmarkRow& values);

  // Deletes the specified rows and returns the number of the deleted rows in
  // |deleted_count|.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // if |selection| is empty all history and bookmarks are deleted.
  bool DeleteHistoryAndBookmarks(
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      int* deleted_count);

  // Deletes the matched history, returns true on success, false on error.
  // The number of deleted row is returned in |deleted_count|.
  // The url row is kept and the visit count is reset if the matched url
  // is bookmarked.
  bool DeleteHistory(const std::string& selection,
                     const std::vector<base::string16>& selection_args,
                     int* deleted_count);

  // SearchTerms --------------------------------------------------------------
  //
  // Returns the result of the given query.
  // |projections| specifies the result columns, can not be empty, otherwise
  // NULL is returned.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| the SQL ORDER clause.
  AndroidStatement* QuerySearchTerms(
      const std::vector<SearchRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order);

  // Runs the given update and returns the number of updated rows in
  // |update_count| and return true, false returned if there is any error.
  //
  // |row| is the value need to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  bool UpdateSearchTerms(const SearchRow& row,
                         const std::string& selection,
                         const std::vector<base::string16>& selection_args,
                         int* update_count);

  // Inserts the given valus and return the SearchTermID of inserted row.
  SearchTermID InsertSearchTerm(const SearchRow& values);

  // Deletes the matched rows and the number of deleted rows is returned in
  // |deleted_count|.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  //
  // if |selection| is empty all search be deleted.
  bool DeleteSearchTerms(const std::string& selection,
                         const std::vector<base::string16>& selection_args,
                         int * deleted_count);

 private:
  friend class AndroidProviderBackendTest;

  FRIEND_TEST_ALL_PREFIXES(AndroidProviderBackendTest, UpdateTables);
  FRIEND_TEST_ALL_PREFIXES(AndroidProviderBackendTest, UpdateSearchTermTable);

  typedef std::list<base::OnceClosure> HistoryNotifications;

  // The scoped transaction for AndroidProviderBackend.
  //
  // The new transactions are started automatically in both history and
  // favicon database and could be a nesting transaction, if so, rolling back
  // of this transaction will cause the exsting and subsequent nesting
  // transactions failed.
  //
  // Commit() is used to commit the transaction, otherwise the transaction will
  // be rolled back when the object is out of scope. This transaction could
  // failed even the commit() is called if it is in a transaction that has been
  // rolled back or the subsequent transaction in the same outermost
  // transaction would be rolled back latter.
  //
  class ScopedTransaction {
   public:
    ScopedTransaction(HistoryDatabase* history_db,
                      favicon::FaviconDatabase* favicon_db);
    ~ScopedTransaction();

    // Commit the transaction.
    void Commit();

   private:
    HistoryDatabase* history_db_;
    favicon::FaviconDatabase* favicon_db_;
    // Whether the transaction was committed.
    bool committed_;
    // The count of the nested transaction in history database.
    const int history_transaction_nesting_;
    // The count of the nested transaction in favicon database.
    const int favicon_transaction_nesting_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTransaction);
  };

  // Runs the given update and returns the number of updated rows in
  // |update_count| and return true on success, false on error.
  //
  // The notifications are returned in |notifications| and the ownership of them
  // is transfered to caller.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  bool UpdateHistoryAndBookmarks(const HistoryAndBookmarkRow& row,
                       const std::string& selection,
                       const std::vector<base::string16>& selection_args,
                       int* update_count,
                       HistoryNotifications* notifications);

  // Inserts the given values and returns the URLID of the inserted row.
  // The notifications are returned in |notifications| and the ownership of them
  // is transfered to caller.
  // The EnsureInitializedAndUpdated() will not be invoked if the
  // |ensure_initialized_and_updated| is false.
  AndroidURLID InsertHistoryAndBookmark(const HistoryAndBookmarkRow& values,
                                        bool ensure_initialized_and_updated,
                                        HistoryNotifications* notifications);

  // Deletes the specified rows and returns the number of the deleted rows in
  // |deleted_count|.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // The notifications are returned in |notifications| and the ownership of them
  // is transfered to the caller.
  // if |selection| is empty all history and bookmarks are deleted.
  bool DeleteHistoryAndBookmarks(
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      int* deleted_count,
      HistoryNotifications* notifications);

  // Deletes the matched history, returns true on success, false on error.
  // The number of deleted row is returned in |deleted_count|.
  // The notifications are returned in |notifications| and the ownership of them
  // is transfered to caller.
  // The url row is kept and the visit is reset if the matched url is
  // bookmarked.
  bool DeleteHistory(const std::string& selection,
                     const std::vector<base::string16>& selection_args,
                     int* deleted_count,
                     HistoryNotifications* notifications);

  // Initializes and updates tables if necessary.
  bool EnsureInitializedAndUpdated();

  // Initializes AndroidProviderBackend.
  bool Init();

  // Update android_urls and bookmark_cache table if it is necessary.
  bool UpdateTables();

  // Update the android_urls and bookmark_cache for visited urls.
  bool UpdateVisitedURLs();

  // Update the android_urls for removed urls.
  bool UpdateRemovedURLs();

  // Update the bookmark_cache table with bookmarks.
  bool UpdateBookmarks();

  // Update the bookmark_cache table for favicon.
  bool UpdateFavicon();

  // Update the search_term table
  bool UpdateSearchTermTable();

  // Append the specified result columns in |projections| to the given
  // |result_column|.
  // To support the lazy binding, the index of favicon column will be
  // returned if it exists, otherwise returns -1.
  int AppendBookmarkResultColumn(
      const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
      std::string* result_column);

  // Append the specified search result columns in |projections| to the given
  // |result_column|.
  void AppendSearchResultColumn(
      const std::vector<SearchRow::ColumnID>& projections,
      std::string* result_column);

  // Runs the given query on history_bookmark virtual table and returns true if
  // succeeds, the selected URLID and url are returned in |rows|.
  bool GetSelectedURLs(const std::string& selection,
                       const std::vector<base::string16>& selection_args,
                       TableIDRows* rows);

  // Runs the given query on search_terms table and returns true on success,
  // The selected search term are returned in |rows|.
  typedef std::vector<base::string16> SearchTerms;
  bool GetSelectedSearchTerms(const std::string& selection,
                              const std::vector<base::string16>& selection_args,
                              SearchTerms* rows);

  // Simulates update url by deleting the previous URL and creating a new one.
  // Return true on success.
  bool SimulateUpdateURL(const HistoryAndBookmarkRow& row,
                         const TableIDRows& ids,
                         HistoryNotifications* notifications);

  // Query bookmark without sync the tables. It should be used after syncing
  // tables.
  AndroidStatement* QueryHistoryAndBookmarksInternal(
      const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order);

  // Delete the given urls' history, returns true on success, or false on error.
  // If |delete_bookmarks| is set, the bookmarks are deleted as well.
  // The notifications are returned in |notifications| and the ownership of them
  // is transfered to caller.
  bool DeleteHistoryInternal(const TableIDRows& urls,
                             bool delete_bookmarks,
                             HistoryNotifications* notifications);

  // Broadcasts |notifications|.  Broadcasting takes ownership of the
  // notifications, so on return |notifications| will be empty.
  void BroadcastNotifications(HistoryNotifications* notifications);

  // Add the search term from the given |values|. It will add the values.url()
  // in the urls table if it doesn't exist, insert visit in the visits table,
  // also add keyword in keyword_search_term.
  bool AddSearchTerm(const SearchRow& values);

  // SQLHandlers for different tables.
  std::unique_ptr<SQLHandler> urls_handler_;
  std::unique_ptr<SQLHandler> visit_handler_;
  std::unique_ptr<SQLHandler> android_urls_handler_;
  std::unique_ptr<SQLHandler> favicon_handler_;
  std::unique_ptr<SQLHandler> bookmark_model_handler_;

  // The vector of all handlers
  std::vector<SQLHandler*> sql_handlers_;

  // Android cache database filename.
  const base::FilePath android_cache_db_filename_;

  // The history db's connection.
  sql::Database* db_;

  HistoryDatabase* history_db_;

  favicon::FaviconDatabase* favicon_db_;

  HistoryBackendClient* backend_client_;

  // Whether AndroidProviderBackend has been initialized.
  bool initialized_;

  HistoryBackendNotifier* notifier_;

  DISALLOW_COPY_AND_ASSIGN(AndroidProviderBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_
