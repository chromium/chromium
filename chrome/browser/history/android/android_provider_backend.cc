// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_provider_backend.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/history/android/bookmark_model_sql_handler.h"
#include "components/history/core/browser/android/android_time.h"
#include "components/history/core/browser/android/android_urls_sql_handler.h"
#include "components/history/core/browser/android/favicon_sql_handler.h"
#include "components/history/core/browser/android/urls_sql_handler.h"
#include "components/history/core/browser/android/visit_sql_handler.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/thumbnail_database.h"

namespace history {


// Helpers --------------------------------------------------------------------

namespace {

const char kAndroidProviderBackendUserDataKey[] = "AndroidProviderBackendKey";

const char kVirtualHistoryAndBookmarkTable[] =
    "SELECT android_urls.id AS _id, "
        "android_cache_db.bookmark_cache.created_time AS created, "
        "urls.title AS title, android_urls.raw_url AS url, "
        "urls.visit_count AS visits, "
        "android_cache_db.bookmark_cache.last_visit_time AS date, "
        "android_cache_db.bookmark_cache.bookmark AS bookmark, "
        "android_cache_db.bookmark_cache.favicon_id AS favicon, "
        "urls.id AS url_id, urls.url AS urls_url, "
    // TODO (michaelbai) : Remove folder column once we remove it from Android
    // framework.
    // Android framework assumes 'folder' column exist in the table, the row is
    // the bookmark once folder is 0, though it is not part of public API, it
    // has to be added and set as 0 when the row is bookmark.
        "(CASE WHEN android_cache_db.bookmark_cache.bookmark IS 0 "
        "THEN 1 ELSE 0 END) as folder "
    "FROM (android_urls JOIN urls on (android_urls.url_id = urls.id) "
        "LEFT JOIN android_cache_db.bookmark_cache "
        "on (android_urls.url_id = android_cache_db.bookmark_cache.url_id))";

const char kURLUpdateClause[] =
    "SELECT urls.id, urls.last_visit_time, created_time, urls.url "
    "FROM urls LEFT JOIN "
        "(SELECT url as visit_url, min(visit_time) as created_time"
        " FROM visits GROUP BY url) ON (visit_url = urls.id) ";

const char kSearchTermUpdateClause[] =
    "SELECT keyword_search_terms.term, max(urls.last_visit_time) "
    "FROM keyword_search_terms JOIN urls ON "
        "(keyword_search_terms.url_id = urls.id) "
    "GROUP BY keyword_search_terms.term";

void BindStatement(const std::vector<base::string16>& selection_args,
                   sql::Statement* statement,
                   int* col_index) {
  for (std::vector<base::string16>::const_iterator i = selection_args.begin();
       i != selection_args.end(); ++i) {
    // Using the same method as Android, binding all argument as String.
    statement->BindString16(*col_index, *i);
    ++(*col_index);
  }
}

bool IsHistoryAndBookmarkRowValid(const HistoryAndBookmarkRow& row) {
  // The caller should make sure both/neither Raw URL and/nor URL should be set.
  DCHECK(row.is_value_set_explicitly(HistoryAndBookmarkRow::RAW_URL) ==
         row.is_value_set_explicitly(HistoryAndBookmarkRow::URL));

  // The following cases are checked:
  // a. Last visit time or created time is large than now.
  // b. Last visit time is less than created time.
  // c. Created time and last visit time is different, but visit count is less
  //    than 2.
  // d. The difference between created and last visit time is less than
  //    visit_count.
  // e. Visit count is 0 or 1 and both last visit time and created time are set
  //    explicitly, but the time is different or created time is not UnixEpoch.
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::LAST_VISIT_TIME) &&
      row.last_visit_time() > base::Time::Now())
    return false;

  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED) &&
      row.created() > base::Time::Now())
    return false;

  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::LAST_VISIT_TIME) &&
      row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED)) {
    if (row.created() > row.last_visit_time())
      return false;

    if (row.is_value_set_explicitly(HistoryAndBookmarkRow::VISIT_COUNT) &&
        row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED) &&
        row.is_value_set_explicitly(HistoryAndBookmarkRow::LAST_VISIT_TIME)) {
      if (row.created() != row.last_visit_time() &&
          row.created() != base::Time::UnixEpoch() &&
          (row.visit_count() == 0 || row.visit_count() == 1))
        return false;

      if (row.last_visit_time().ToInternalValue() -
          row.created().ToInternalValue() < row.visit_count())
        return false;
    }
  }
  return true;
}

void RunNotifyFaviconChanged(HistoryBackendNotifier* notifier,
                             std::unique_ptr<std::set<GURL>> urls) {
  notifier->NotifyFaviconsChanged(*urls, GURL());
}

void RunNotifyURLsModified(HistoryBackendNotifier* notifier,
                           std::unique_ptr<URLRows> rows) {
  // All modifications from the android UI are caused by user action and not by
  // expiration.
  notifier->NotifyURLsModified(*rows, /*is_from_expiration=*/false);
}

void RunNotifyURLsDeleted(HistoryBackendNotifier* notifier,
                          std::unique_ptr<URLRows> rows) {
  notifier->NotifyURLsDeleted(DeletionInfo::ForUrls(*rows, {}));
}

}  // namespace

// AndroidProviderBackend::ScopedTransaction ----------------------------------

AndroidProviderBackend::ScopedTransaction::ScopedTransaction(
    HistoryDatabase* history_db,
    ThumbnailDatabase* thumbnail_db)
    : history_db_(history_db),
      thumbnail_db_(thumbnail_db),
      committed_(false),
      history_transaction_nesting_(history_db_->transaction_nesting()),
      thumbnail_transaction_nesting_(
          thumbnail_db_ ? thumbnail_db_->transaction_nesting() : 0) {
  // Commit all existing transactions since the AndroidProviderBackend's
  // transaction is very like to be rolled back when compared with the others.
  // The existing transactions have been scheduled to commit by
  // ScheduleCommit in HistoryBackend and the same number of transaction
  // will be created after this scoped transaction ends, there should have no
  // issue to directly commit all transactions here.
  int count = history_transaction_nesting_;
  while (count--)
    history_db_->CommitTransaction();
  history_db_->BeginTransaction();

  if (thumbnail_db_) {
    count = thumbnail_transaction_nesting_;
    while (count--)
      thumbnail_db_->CommitTransaction();
    thumbnail_db_->BeginTransaction();
  }
}

AndroidProviderBackend::ScopedTransaction::~ScopedTransaction() {
  if (!committed_) {
    history_db_->RollbackTransaction();
    if (thumbnail_db_)
      thumbnail_db_->RollbackTransaction();
  }
  // There is no transaction now.
  DCHECK_EQ(0, history_db_->transaction_nesting());
  DCHECK(!thumbnail_db_ || 0 == thumbnail_db_->transaction_nesting());

  int count = history_transaction_nesting_;
  while (count--)
    history_db_->BeginTransaction();

  if (thumbnail_db_) {
    count = thumbnail_transaction_nesting_;
    while (count--)
      thumbnail_db_->BeginTransaction();
  }
}

void AndroidProviderBackend::ScopedTransaction::Commit() {
  DCHECK(!committed_);
  history_db_->CommitTransaction();
  if (thumbnail_db_)
    thumbnail_db_->CommitTransaction();
  committed_ = true;
}


// AndroidProviderBackend -----------------------------------------------------

AndroidProviderBackend::AndroidProviderBackend(
    const base::FilePath& db_name,
    HistoryDatabase* history_db,
    ThumbnailDatabase* thumbnail_db,
    HistoryBackendClient* backend_client,
    HistoryBackendNotifier* notifier)
    : android_cache_db_filename_(db_name),
      db_(&history_db->GetDB()),
      history_db_(history_db),
      thumbnail_db_(thumbnail_db),
      backend_client_(backend_client),
      initialized_(false),
      notifier_(notifier) {
  DCHECK(notifier_);
}

AndroidProviderBackend::~AndroidProviderBackend() {
}

// static
const void* AndroidProviderBackend::GetUserDataKey() {
  return kAndroidProviderBackendUserDataKey;
}

// static
AndroidProviderBackend* AndroidProviderBackend::FromHistoryBackend(
    HistoryBackend* history_backend) {
  DCHECK(history_backend);
  return static_cast<AndroidProviderBackend*>(
      history_backend->GetUserData(GetUserDataKey()));
}

AndroidStatement* AndroidProviderBackend::QueryHistoryAndBookmarks(
    const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order) {
  if (projections.empty())
    return NULL;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!EnsureInitializedAndUpdated())
    return NULL;

  transaction.Commit();

  return QueryHistoryAndBookmarksInternal(projections, selection,
                                          selection_args, sort_order);
}

bool AndroidProviderBackend::UpdateHistoryAndBookmarks(
    const HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* updated_count) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!UpdateHistoryAndBookmarks(row, selection, selection_args, updated_count,
                                 &notifications))
    return false;

  transaction.Commit();
  BroadcastNotifications(&notifications);
  return true;
}

AndroidURLID AndroidProviderBackend::InsertHistoryAndBookmark(
    const HistoryAndBookmarkRow& values) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  AndroidURLID id = InsertHistoryAndBookmark(values, true, &notifications);
  if (!id)
    return 0;

  transaction.Commit();
  BroadcastNotifications(&notifications);
  return id;
}

bool AndroidProviderBackend::DeleteHistoryAndBookmarks(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* deleted_count) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!DeleteHistoryAndBookmarks(selection, selection_args, deleted_count,
                                 &notifications))
    return false;

  transaction.Commit();
  BroadcastNotifications(&notifications);
  return true;
}

bool AndroidProviderBackend::DeleteHistory(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* deleted_count) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!DeleteHistory(selection, selection_args, deleted_count, &notifications))
    return false;

  transaction.Commit();
  BroadcastNotifications(&notifications);
  return true;
}

bool AndroidProviderBackend::UpdateHistoryAndBookmarks(
    const HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* updated_count,
    HistoryNotifications* notifications) {
  if (!IsHistoryAndBookmarkRowValid(row))
    return false;

  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::ID))
    return false;

  if (!EnsureInitializedAndUpdated())
    return false;

  TableIDRows ids_set;
  if (!GetSelectedURLs(selection, selection_args, &ids_set))
    return false;

  if (ids_set.empty()) {
    *updated_count = 0;
    return true;
  }

  // URL can not be updated, we simulate the update.
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::URL)) {
    // Only one row's URL can be updated at a time as we can't have multiple
    // rows have the same URL.
    if (ids_set.size() != 1)
      return false;

    HistoryAndBookmarkRow new_row = row;
    if (!SimulateUpdateURL(new_row, ids_set, notifications))
      return false;
    *updated_count = 1;
    return true;
  }

  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if ((*i)->HasColumnIn(row)) {
      if (!(*i)->Update(row, ids_set))
        return false;
    }
  }
  *updated_count = ids_set.size();

  std::unique_ptr<URLRows> changed_urls;
  std::unique_ptr<std::set<GURL>> favicon;

  for (const auto& id : ids_set) {
    if (row.is_value_set_explicitly(HistoryAndBookmarkRow::TITLE) ||
        row.is_value_set_explicitly(HistoryAndBookmarkRow::VISIT_COUNT) ||
        row.is_value_set_explicitly(HistoryAndBookmarkRow::LAST_VISIT_TIME)) {
      URLRow url_row;
      if (!history_db_->GetURLRow(id.url_id, &url_row))
        return false;
      if (!changed_urls)
        changed_urls.reset(new URLRows);
      changed_urls->push_back(url_row);
    }
    if (thumbnail_db_ &&
        row.is_value_set_explicitly(HistoryAndBookmarkRow::FAVICON)) {
      if (!favicon)
        favicon.reset(new std::set<GURL>);
      favicon->insert(id.url);
    }
  }

  if (changed_urls) {
    DCHECK(!changed_urls->empty());
    notifications->push_back(
        base::Bind(&RunNotifyURLsModified,
                   base::Unretained(notifier_),
                   base::Passed(&changed_urls)));
  }

  if (favicon) {
    DCHECK(!favicon->empty());
    notifications->push_back(
        base::Bind(&RunNotifyFaviconChanged,
                   base::Unretained(notifier_),
                   base::Passed(&favicon)));
  }

  return true;
}

AndroidURLID AndroidProviderBackend::InsertHistoryAndBookmark(
    const HistoryAndBookmarkRow& values,
    bool ensure_initialized_and_updated,
    HistoryNotifications* notifications) {
  if (!IsHistoryAndBookmarkRowValid(values))
    return false;

  if (ensure_initialized_and_updated && !EnsureInitializedAndUpdated())
    return 0;

  DCHECK(values.is_value_set_explicitly(HistoryAndBookmarkRow::URL));
  // Make a copy of values as we need change it during insert.
  HistoryAndBookmarkRow row = values;
  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if (!(*i)->Insert(&row))
      return 0;
  }

  URLRow url_row;
  if (!history_db_->GetURLRow(row.url_id(), &url_row))
    return false;

  std::unique_ptr<URLRows> changed_urls(new URLRows);
  changed_urls->push_back(url_row);

  std::unique_ptr<std::set<GURL>> favicon;
  // No favicon should be changed if the thumbnail_db_ is not available.
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::FAVICON) &&
      row.favicon_valid() && thumbnail_db_) {
    favicon.reset(new std::set<GURL>);
    favicon->insert(url_row.url());
  }

  notifications->push_back(
      base::Bind(&RunNotifyURLsModified,
                 base::Unretained(notifier_),
                 base::Passed(&changed_urls)));

  if (favicon) {
    DCHECK(!favicon->empty());
    notifications->push_back(
        base::Bind(&RunNotifyFaviconChanged,
                   base::Unretained(notifier_),
                   base::Passed(&favicon)));
  }

  return row.id();
}

bool AndroidProviderBackend::DeleteHistoryAndBookmarks(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int * deleted_count,
    HistoryNotifications* notifications) {
  if (!EnsureInitializedAndUpdated())
    return false;

  TableIDRows ids_set;
  if (!GetSelectedURLs(selection, selection_args, &ids_set))
    return false;

  if (ids_set.empty()) {
    *deleted_count = 0;
    return true;
  }

  if (!DeleteHistoryInternal(ids_set, true, notifications))
    return false;

  *deleted_count = ids_set.size();

  return true;
}

bool AndroidProviderBackend::DeleteHistory(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* deleted_count,
    HistoryNotifications* notifications) {
  if (!EnsureInitializedAndUpdated())
    return false;

  TableIDRows ids_set;
  if (!GetSelectedURLs(selection, selection_args, &ids_set))
    return false;

  if (ids_set.empty()) {
    *deleted_count = 0;
    return true;
  }

  *deleted_count = ids_set.size();

  // Get the bookmarked rows.
  std::vector<HistoryAndBookmarkRow> bookmarks;
  for (TableIDRows::const_iterator i = ids_set.begin(); i != ids_set.end();
       ++i) {
    if (i->bookmarked) {
      AndroidURLRow android_url_row;
      if (!history_db_->GetAndroidURLRow(i->url_id, &android_url_row))
        return false;
      HistoryAndBookmarkRow row;
      row.set_raw_url(android_url_row.raw_url);
      row.set_url(i->url);
      // Set the visit time to the UnixEpoch since that's when the Android
      // system time starts. The Android have a CTS testcase for this.
      row.set_last_visit_time(base::Time::UnixEpoch());
      row.set_visit_count(0);
      // We don't want to change the bookmark model, so set_is_bookmark() is
      // not called.
      bookmarks.push_back(row);
    }
  }

  // Don't delete the bookmark from bookmark model when deleting the history.
  if (!DeleteHistoryInternal(ids_set, false, notifications))
    return false;

  for (std::vector<HistoryAndBookmarkRow>::const_iterator i = bookmarks.begin();
       i != bookmarks.end(); ++i) {
    // Don't update the tables, otherwise, the bookmarks will be added to
    // database during UpdateBookmark(), then the insertion will fail.
    // We can't rely on UpdateBookmark() to insert the bookmarks into history
    // database as the raw_url will be lost.
    if (!InsertHistoryAndBookmark(*i, false, notifications))
      return false;
  }
  return true;
}

AndroidStatement* AndroidProviderBackend::QuerySearchTerms(
    const std::vector<SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order) {
  if (projections.empty())
    return NULL;

  if (!EnsureInitializedAndUpdated())
    return NULL;

  std::string sql;
  sql.append("SELECT ");
  AppendSearchResultColumn(projections, &sql);
  sql.append(" FROM android_cache_db.search_terms ");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  if (!sort_order.empty()) {
    sql.append(" ORDER BY ");
    sql.append(sort_order);
  }

  std::unique_ptr<sql::Statement> statement(
      new sql::Statement(db_->GetUniqueStatement(sql.c_str())));
  int count = 0;
  BindStatement(selection_args, statement.get(), &count);
  if (!statement->is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return NULL;
  }
  sql::Statement* result = statement.release();
  return new AndroidStatement(result, -1);
}

bool AndroidProviderBackend::UpdateSearchTerms(
    const SearchRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int* update_count) {
  if (!EnsureInitializedAndUpdated())
    return false;

  SearchTerms search_terms;
  if (!GetSelectedSearchTerms(selection, selection_args, &search_terms))
    return false;

  // We can not update search term if multiple row selected.
  if (row.is_value_set_explicitly(SearchRow::SEARCH_TERM) &&
      search_terms.size() > 1)
    return false;

  *update_count = search_terms.size();

  if (search_terms.empty())
    return true;

  if (row.is_value_set_explicitly(SearchRow::SEARCH_TERM)) {
    SearchTermRow search_term_row;
    SearchRow search_row = row;
    if (!history_db_->GetSearchTerm(search_terms[0], &search_term_row))
      return false;

    search_term_row.term = search_row.search_term();
    if (!search_row.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      search_row.set_search_time(search_term_row.last_visit_time);
    else
      search_term_row.last_visit_time = search_row.search_time();

    // Delete the original search term.
    if (!history_db_->DeleteKeywordSearchTerm(search_terms[0]))
      return false;

    // Add the new one.
    if (!AddSearchTerm(search_row))
      return false;

    // Update the cache table so the id will not be changed.
    if (!history_db_->UpdateSearchTerm(search_term_row.id, search_term_row))
      return false;

     return true;
  }

  for (SearchTerms::const_iterator i = search_terms.begin();
       i != search_terms.end(); ++i) {
    SearchTermRow search_term_row;
    if (!history_db_->GetSearchTerm(*i, &search_term_row))
      return false;

    // Check whether the given search time less than the existing one.
    if (search_term_row.last_visit_time > row.search_time())
      return false;

    std::vector<KeywordSearchTermRow> search_term_rows;
    if (!history_db_->GetKeywordSearchTermRows(*i, &search_term_rows) ||
        search_term_rows.empty())
      return false;

    // Actually only search_time update. As there might multiple URLs
    // asocciated with the keyword, Just update the first one's last_visit_time.
    URLRow url_row;
    if (!history_db_->GetURLRow(search_term_rows[0].url_id, &url_row))
      return false;

    HistoryAndBookmarkRow bookmark_row;
    bookmark_row.set_last_visit_time(row.search_time());
    TableIDRow table_id_row;
    table_id_row.url_id = url_row.id();
    TableIDRows table_id_rows;
    table_id_rows.push_back(table_id_row);
    if (!urls_handler_->Update(bookmark_row, table_id_rows))
      return false;

    if (!visit_handler_->Update(bookmark_row, table_id_rows))
      return false;
  }
  return true;
}

SearchTermID AndroidProviderBackend::InsertSearchTerm(
    const SearchRow& values) {
  if (!EnsureInitializedAndUpdated())
    return 0;

  if (!AddSearchTerm(values))
    return 0;

  SearchTermID id = history_db_->GetSearchTerm(values.search_term(), NULL);
  if (!id)
    // Note the passed in Time() will be changed in UpdateSearchTermTable().
    id = history_db_->AddSearchTerm(values.search_term(), base::Time());
  return id;
}

bool AndroidProviderBackend::DeleteSearchTerms(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    int * deleted_count) {
  if (!EnsureInitializedAndUpdated())
    return false;

  SearchTerms rows;
  if (!GetSelectedSearchTerms(selection, selection_args, &rows))
    return false;

  *deleted_count = rows.size();
  if (rows.empty())
    return true;

  for (SearchTerms::const_iterator i = rows.begin(); i != rows.end(); ++i)
    if (!history_db_->DeleteKeywordSearchTerm(*i))
      return false;
  // We don't delete the rows in search_terms table, as once the
  // search_terms table is updated with keyword_search_terms, all
  // keyword cache not found in the keyword_search_terms will be removed.
  return true;
}

bool AndroidProviderBackend::EnsureInitializedAndUpdated() {
  if (!initialized_) {
    if (!Init())
      return false;
  }
  return UpdateTables();
}

bool AndroidProviderBackend::Init() {
  urls_handler_.reset(new UrlsSQLHandler(history_db_));
  visit_handler_.reset(new VisitSQLHandler(history_db_, history_db_));
  android_urls_handler_.reset(new AndroidURLsSQLHandler(history_db_));
  if (thumbnail_db_)
    favicon_handler_.reset(new FaviconSQLHandler(thumbnail_db_));
  bookmark_model_handler_.reset(new BookmarkModelSQLHandler(history_db_));
  // The urls_handler must be pushed first, because the subsequent handlers
  // depend on its output.
  sql_handlers_.push_back(urls_handler_.get());
  sql_handlers_.push_back(visit_handler_.get());
  sql_handlers_.push_back(android_urls_handler_.get());
  if (favicon_handler_.get())
    sql_handlers_.push_back(favicon_handler_.get());
  sql_handlers_.push_back(bookmark_model_handler_.get());

  if (!history_db_->CreateAndroidURLsTable())
    return false;
  if (sql::INIT_OK != history_db_->InitAndroidCacheDatabase(
          android_cache_db_filename_))
    return false;
  initialized_ = true;
  return true;
}

bool AndroidProviderBackend::UpdateTables() {
  if (!UpdateVisitedURLs()) {
    LOG(ERROR) << "Update of the visisted URLS failed";
    return false;
  }

  if (!UpdateRemovedURLs()) {
    LOG(ERROR) << "Update of the removed URLS failed";
    return false;
  }

  if (!UpdateBookmarks()) {
    LOG(ERROR) << "Update of the bookmarks failed";
    return false;
  }

  if (!UpdateFavicon()) {
    LOG(ERROR) << "Update of the icons failed";
    return false;
  }

  if (!UpdateSearchTermTable()) {
    LOG(ERROR) << "Update of the search_terms failed";
    return false;
  }
  return true;
}

bool AndroidProviderBackend::UpdateVisitedURLs() {
  std::string sql(kURLUpdateClause);
  sql.append("WHERE urls.id NOT IN (SELECT url_id FROM android_urls)");
  sql::Statement urls_statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                        sql.c_str()));
  if (!urls_statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }

  while (urls_statement.Step()) {
    if (history_db_->GetAndroidURLRow(urls_statement.ColumnInt64(0), NULL))
      continue;
    if (!history_db_->AddAndroidURLRow(urls_statement.ColumnString(3),
                                       urls_statement.ColumnInt64(0)))
      return false;
  }

  if (!history_db_->ClearAllBookmarkCache())
    return false;

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   kURLUpdateClause));
  while (statement.Step()) {
    // The last_visit_time and the created time should be same when the visit
    // count is 0, this behavior is also required by the Android CTS.
    // The created_time could be set to the last_visit_time only when the type
    // of the 'created' column is NULL because the left join is used in query
    // and there is no row in the visit table when the visit count is 0.
    base::Time last_visit_time =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
    base::Time created_time = last_visit_time;

    if (statement.GetColumnType(2) != sql::ColumnType::kNull)
      created_time = base::Time::FromInternalValue(statement.ColumnInt64(2));

    if (!history_db_->AddBookmarkCacheRow(created_time, last_visit_time,
                                          statement.ColumnInt64(0)))
      return false;
  }
  return true;
}

bool AndroidProviderBackend::UpdateRemovedURLs() {
  return history_db_->DeleteUnusedAndroidURLs();
}

bool AndroidProviderBackend::UpdateBookmarks() {
  if (backend_client_ == NULL) {
    LOG(ERROR) << "HistoryClient is not available";
    return false;
  }

  std::vector<URLAndTitle> pinned_urls = backend_client_->GetPinnedURLs();

  if (pinned_urls.empty())
    return true;

  std::vector<URLID> url_ids;
  for (std::vector<URLAndTitle>::const_iterator i = pinned_urls.begin();
       i != pinned_urls.end(); ++i) {
    URLID url_id = history_db_->GetRowForURL(i->url, NULL);
    if (url_id == 0) {
      URLRow url_row(i->url);
      url_row.set_title(i->title);
      // Set the visit time to the UnixEpoch since that's when the Android
      // system time starts. The Android have a CTS testcase for this.
      url_row.set_last_visit(base::Time::UnixEpoch());
      url_row.set_hidden(true);
      url_id = history_db_->AddURL(url_row);
      if (url_id == 0) {
        LOG(ERROR) << "Can not add url for the new bookmark";
        return false;
      }
      if (!history_db_->AddAndroidURLRow(i->url.spec(), url_id))
        return false;
      if (!history_db_->AddBookmarkCacheRow(base::Time::UnixEpoch(),
                                            base::Time::UnixEpoch(), url_id))
        return false;
    }
    url_ids.push_back(url_id);
  }

  return history_db_->MarkURLsAsBookmarked(url_ids);
}

bool AndroidProviderBackend::UpdateFavicon() {
  ThumbnailDatabase::IconMappingEnumerator enumerator;

  // We want the AndroidProviderBackend run without thumbnail_db_
  if (!thumbnail_db_)
    return true;

  if (!thumbnail_db_->InitIconMappingEnumerator(
          favicon_base::IconType::kFavicon, &enumerator))
    return false;

  IconMapping icon_mapping;
  while (enumerator.GetNextIconMapping(&icon_mapping)) {
    URLID url_id = history_db_->GetRowForURL(icon_mapping.page_url, NULL);
    if (url_id == 0) {
      LOG(ERROR) << "Can not find favicon's page url";
      continue;
    }
    history_db_->SetFaviconID(url_id, icon_mapping.icon_id);
  }
  return true;
}

bool AndroidProviderBackend::UpdateSearchTermTable() {
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   kSearchTermUpdateClause));
  while (statement.Step()) {
    base::string16 term = statement.ColumnString16(0);
    base::Time last_visit_time =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
    SearchTermRow search_term_row;
    if (history_db_->GetSearchTerm(term, &search_term_row)) {
      if (search_term_row.last_visit_time != last_visit_time) {
        search_term_row.last_visit_time = last_visit_time;
        if (!history_db_->UpdateSearchTerm(search_term_row.id, search_term_row))
          return false;
      }
    } else {
      if (!history_db_->AddSearchTerm(term, last_visit_time))
        return false;
    }
  }
  if (!history_db_->DeleteUnusedSearchTerms())
    return false;

  return true;
}

int AndroidProviderBackend::AppendBookmarkResultColumn(
    const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
    std::string* result_column) {
  int replaced_index = -1;
  // Attach the projections
  bool first = true;
  int index = 0;
  for (std::vector<HistoryAndBookmarkRow::ColumnID>::const_iterator i =
           projections.begin(); i != projections.end(); ++i) {
    if (first)
      first = false;
    else
      result_column->append(", ");

    if (*i == HistoryAndBookmarkRow::FAVICON)
      replaced_index = index;

    result_column->append(HistoryAndBookmarkRow::GetAndroidName(*i));
    index++;
  }
  return replaced_index;
}

bool AndroidProviderBackend::GetSelectedURLs(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    TableIDRows* rows) {
  std::string sql("SELECT url_id, urls_url, bookmark FROM (");
  sql.append(kVirtualHistoryAndBookmarkTable);
  sql.append(" )");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  int count = 0;
  BindStatement(selection_args, &statement, &count);
  if (!statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }
  while (statement.Step()) {
    TableIDRow row;
    row.url_id = statement.ColumnInt64(0);
    row.url = GURL(statement.ColumnString(1));
    row.bookmarked = statement.ColumnBool(2);
    rows->push_back(row);
  }
  return true;
}

bool AndroidProviderBackend::GetSelectedSearchTerms(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    SearchTerms* rows) {
  std::string sql("SELECT search "
                  "FROM android_cache_db.search_terms ");
  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  int count = 0;
  BindStatement(selection_args, &statement, &count);
  if (!statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }
  while (statement.Step()) {
    rows->push_back(statement.ColumnString16(0));
  }
  return true;
}

void AndroidProviderBackend::AppendSearchResultColumn(
    const std::vector<SearchRow::ColumnID>& projections,
    std::string* result_column) {
  bool first = true;
  int index = 0;
  for (std::vector<SearchRow::ColumnID>::const_iterator i =
           projections.begin(); i != projections.end(); ++i) {
    if (first)
      first = false;
    else
      result_column->append(", ");

    result_column->append(SearchRow::GetAndroidName(*i));
    index++;
  }
}

bool AndroidProviderBackend::SimulateUpdateURL(
    const HistoryAndBookmarkRow& row,
    const TableIDRows& ids,
    HistoryNotifications* notifications) {
  DCHECK(ids.size() == 1);
  // URL can not be updated, we simulate the update by deleting the old URL
  // and inserting the new one; We do update the android_urls table as the id
  // need to keep same.

  // Find all columns value of the current URL.
  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::LAST_VISIT_TIME);
  projections.push_back(HistoryAndBookmarkRow::CREATED);
  projections.push_back(HistoryAndBookmarkRow::VISIT_COUNT);
  projections.push_back(HistoryAndBookmarkRow::TITLE);
  projections.push_back(HistoryAndBookmarkRow::FAVICON);
  projections.push_back(HistoryAndBookmarkRow::BOOKMARK);

  std::ostringstream oss;
  oss << "url_id = " << ids[0].url_id;

  std::unique_ptr<AndroidStatement> statement;
  statement.reset(QueryHistoryAndBookmarksInternal(projections, oss.str(),
      std::vector<base::string16>(), std::string()));
  if (!statement.get() || !statement->statement()->Step())
    return false;

  HistoryAndBookmarkRow new_row;
  new_row.set_last_visit_time(FromDatabaseTime(
      statement->statement()->ColumnInt64(0)));
  new_row.set_created(FromDatabaseTime(
      statement->statement()->ColumnInt64(1)));
  new_row.set_visit_count(statement->statement()->ColumnInt(2));
  new_row.set_title(statement->statement()->ColumnString16(3));

  URLRow old_url_row;
  if (!history_db_->GetURLRow(ids[0].url_id, &old_url_row))
    return false;

  std::unique_ptr<std::set<GURL>> favicons;
  std::unique_ptr<URLRows> deleted_rows(new URLRows);
  deleted_rows->push_back(old_url_row);

  favicon_base::FaviconID favicon_id = statement->statement()->ColumnInt64(4);
  if (favicon_id) {
    std::vector<FaviconBitmap> favicon_bitmaps;
    if (!thumbnail_db_ ||
        !thumbnail_db_->GetFaviconBitmaps(favicon_id, &favicon_bitmaps))
      return false;
   scoped_refptr<base::RefCountedMemory> bitmap_data =
       favicon_bitmaps[0].bitmap_data;
   if (bitmap_data.get() && bitmap_data->size())
      new_row.set_favicon(bitmap_data);
   if (!favicons)
     favicons.reset(new std::set<GURL>);
   favicons->insert(old_url_row.url());
   favicons->insert(row.url());
  }
  new_row.set_is_bookmark(statement->statement()->ColumnBool(5));

  // The SQLHandler vector is not used here because the row in android_url
  // shouldn't be deleted, we need keep the AndroidUIID unchanged, so it
  // appears update to the client.
  if (!urls_handler_->Delete(ids))
    return false;

  if (!visit_handler_->Delete(ids))
    return false;

  if (favicon_handler_ && !favicon_handler_->Delete(ids))
    return false;

  if (!bookmark_model_handler_->Delete(ids))
    return false;

  new_row.set_url(row.url());
  new_row.set_raw_url(row.raw_url());
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::LAST_VISIT_TIME))
    new_row.set_last_visit_time(row.last_visit_time());
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED))
    new_row.set_created(row.created());
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::VISIT_COUNT))
    new_row.set_visit_count(row.visit_count());
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::TITLE))
    new_row.set_title(row.title());
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::FAVICON)) {
    new_row.set_favicon(row.favicon());
    favicons.reset(new std::set<GURL>);
    favicons->insert(new_row.url());
  }
  if (row.is_value_set_explicitly(HistoryAndBookmarkRow::BOOKMARK))
    new_row.set_is_bookmark(row.is_bookmark());

  if (!urls_handler_->Insert(&new_row))
    return false;

  if (!visit_handler_->Insert(&new_row))
    return false;

  // Update the current row instead of inserting a new row in android urls
  // table. We need keep the AndroidUIID unchanged, so it appears update
  // to the client.
  if (!android_urls_handler_->Update(new_row, ids))
    return false;

  if (favicon_handler_ && !favicon_handler_->Insert(&new_row))
    return false;

  if (!bookmark_model_handler_->Insert(&new_row))
    return false;

  URLRow new_url_row;
  if (!history_db_->GetURLRow(new_row.url_id(), &new_url_row))
    return false;

  std::unique_ptr<URLRows> changed_urls(new URLRows);
  changed_urls->push_back(new_url_row);

  notifications->push_back(
      base::Bind(&RunNotifyURLsDeleted,
                 base::Unretained(notifier_),
                 base::Passed(&deleted_rows)));
  if (favicons) {
    DCHECK(!favicons->empty());
    notifications->push_back(
        base::Bind(&RunNotifyFaviconChanged,
                   base::Unretained(notifier_),
                   base::Passed(&favicons)));
  }
  notifications->push_back(
      base::Bind(&RunNotifyURLsModified,
                 base::Unretained(notifier_),
                 base::Passed(&changed_urls)));

  return true;
}

AndroidStatement* AndroidProviderBackend::QueryHistoryAndBookmarksInternal(
    const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order) {
  std::string sql;
  sql.append("SELECT ");
  int replaced_index = AppendBookmarkResultColumn(projections, &sql);
  sql.append(" FROM (");
  sql.append(kVirtualHistoryAndBookmarkTable);
  sql.append(")");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  if (!sort_order.empty()) {
    sql.append(" ORDER BY ");
    sql.append(sort_order);
  }

  std::unique_ptr<sql::Statement> statement(
      new sql::Statement(db_->GetUniqueStatement(sql.c_str())));
  int count = 0;
  BindStatement(selection_args, statement.get(), &count);
  if (!statement->is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return NULL;
  }
  sql::Statement* result = statement.release();
  return new AndroidStatement(result, replaced_index);
}

bool AndroidProviderBackend::DeleteHistoryInternal(
    const TableIDRows& urls,
    bool delete_bookmarks,
    HistoryNotifications* notifications) {
  std::unique_ptr<URLRows> deleted_rows;
  std::unique_ptr<std::set<GURL>> favicons;
  for (TableIDRows::const_iterator i = urls.begin(); i != urls.end(); ++i) {
    URLRow url_row;
    if (!history_db_->GetURLRow(i->url_id, &url_row))
      return false;
    if (!deleted_rows)
      deleted_rows.reset(new URLRows);
    deleted_rows->push_back(url_row);
    if (thumbnail_db_ &&
        thumbnail_db_->GetIconMappingsForPageURL(url_row.url(), NULL)) {
      if (!favicons)
        favicons.reset(new std::set<GURL>);
      favicons->insert(url_row.url());
    }
  }

  // Only invoke Delete on the BookmarkModelHandler if we need
  // to delete bookmarks.
  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if ((*i) != bookmark_model_handler_.get() || delete_bookmarks)
      if (!(*i)->Delete(urls))
        return false;
  }

  notifications->push_back(
      base::Bind(&RunNotifyURLsDeleted,
                 base::Unretained(notifier_),
                 base::Passed(&deleted_rows)));
  if (favicons) {
    DCHECK(!favicons->empty());
    notifications->push_back(
        base::Bind(&RunNotifyFaviconChanged,
                   base::Unretained(notifier_),
                   base::Passed(&favicons)));
  }
  return true;
}

void AndroidProviderBackend::BroadcastNotifications(
    HistoryNotifications* notifications) {
  while (!notifications->empty()) {
    notifications->back().Run();
    notifications->pop_back();
  }
}

bool AndroidProviderBackend::AddSearchTerm(const SearchRow& values) {
  DCHECK(values.is_value_set_explicitly(SearchRow::SEARCH_TERM));
  DCHECK(values.is_value_set_explicitly(SearchRow::KEYWORD_ID));
  DCHECK(values.is_value_set_explicitly(SearchRow::URL));

  URLRow url_row;
  HistoryAndBookmarkRow bookmark_row;
  // Android CTS test BrowserTest.testAccessSearches allows insert the same
  // seach term multiple times, and just search time need updated.
  if (history_db_->GetRowForURL(values.url(), &url_row)) {
    // Already exist, Add a visit.
    if (values.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      bookmark_row.set_last_visit_time(values.search_time());
    else
      bookmark_row.set_visit_count(url_row.visit_count() + 1);
    TableIDRows table_id_rows;
    TableIDRow table_id_row;
    table_id_row.url = values.url();
    table_id_row.url_id = url_row.id();
    table_id_rows.push_back(table_id_row);
    if (!urls_handler_->Update(bookmark_row, table_id_rows))
      return false;
    if (!visit_handler_->Update(bookmark_row, table_id_rows))
      return false;

    if (!history_db_->GetKeywordSearchTermRow(url_row.id(), NULL))
      if (!history_db_->SetKeywordSearchTermsForURL(url_row.id(),
               values.keyword_id(), values.search_term()))
        return false;
  } else {
    bookmark_row.set_raw_url(values.url().spec());
    bookmark_row.set_url(values.url());
    if (values.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      bookmark_row.set_last_visit_time(values.search_time());

    if (!urls_handler_->Insert(&bookmark_row))
      return false;

    if (!visit_handler_->Insert(&bookmark_row))
      return false;

    if (!android_urls_handler_->Insert(&bookmark_row))
      return false;

    if (!history_db_->SetKeywordSearchTermsForURL(bookmark_row.url_id(),
                          values.keyword_id(), values.search_term()))
      return false;
  }
  return true;
}

}  // namespace history
