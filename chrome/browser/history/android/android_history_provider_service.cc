// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_provider_service.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/android/android_provider_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"

namespace {

// AndroidProviderTask wraps two callbacks into an HistoryDBTask so that they
// can be passed to HistoryService::ScheduleDBTask. ResultType must be zero
// constructible (i.e. ResultType(0) should build an initialized default value)
// and copyable.
template <typename ResultType>
class AndroidProviderTask : public history::HistoryDBTask {
 public:
  typedef base::Callback<ResultType(history::AndroidProviderBackend*)>
      RequestCallback;
  typedef base::Callback<void(ResultType)> ResultCallback;

  AndroidProviderTask(const RequestCallback& request_cb,
                      const ResultCallback& result_cb)
      : request_cb_(request_cb), result_cb_(result_cb), result_(0) {
    DCHECK(!request_cb_.is_null());
    DCHECK(!result_cb_.is_null());
  }

  ~AndroidProviderTask() override {}

 private:
  // history::HistoryDBTask implementation.
  bool RunOnDBThread(history::HistoryBackend* history_backend,
                     history::HistoryDatabase* db) override {
    history::AndroidProviderBackend* android_provider_backend =
      history::AndroidProviderBackend::FromHistoryBackend(history_backend);
    if (android_provider_backend)
      result_ = request_cb_.Run(android_provider_backend);
    return true;
  }

  void DoneRunOnMainThread() override { result_cb_.Run(result_); }

  RequestCallback request_cb_;
  ResultCallback result_cb_;
  ResultType result_;
};

// Creates an instance of AndroidProviderTask using the two callback and using
// type deduction.
template <typename ResultType>
std::unique_ptr<history::HistoryDBTask> CreateAndroidProviderTask(
    const base::Callback<ResultType(history::AndroidProviderBackend*)>&
        request_cb,
    const base::Callback<void(ResultType)>& result_cb) {
  return std::unique_ptr<history::HistoryDBTask>(
      new AndroidProviderTask<ResultType>(request_cb, result_cb));
}

// History and bookmarks ----------------------------------------------------

// Inserts the given values into android provider backend.
history::AndroidURLID InsertHistoryAndBookmarkAdapter(
    const history::HistoryAndBookmarkRow& row,
    history::AndroidProviderBackend* backend) {
  return backend->InsertHistoryAndBookmark(row);
}

// Runs the given query on android provider backend and returns the result.
//
// |projections| is the vector of the result columns.
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for WHERE clause.
// |sort_order| is the SQL ORDER clause.
history::AndroidStatement* QueryHistoryAndBookmarksAdapter(
    const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    history::AndroidProviderBackend* backend) {
  return backend->QueryHistoryAndBookmarks(projections, selection,
                                           selection_args, sort_order);
}

// Returns the number of row updated by the update query.
//
// |row| is the value to update.
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for the WHERE clause.
int UpdateHistoryAndBookmarksAdapter(
    const history::HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    history::AndroidProviderBackend* backend) {
  int count = 0;
  backend->UpdateHistoryAndBookmarks(row, selection, selection_args, &count);
  return count;
}

// Deletes the specified rows and returns the number of rows deleted.
//
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for the WHERE clause.
//
// If |selection| is empty all history and bookmarks are deleted.
int DeleteHistoryAndBookmarksAdapter(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    history::AndroidProviderBackend* backend) {
  int count = 0;
  backend->DeleteHistoryAndBookmarks(selection, selection_args, &count);
  return count;
}

// Deletes the matched history and returns the number of rows deleted.
int DeleteHistoryAdapter(const std::string& selection,
                         const std::vector<base::string16>& selection_args,
                         history::AndroidProviderBackend* backend) {
  int count = 0;
  backend->DeleteHistory(selection, selection_args, &count);
  return count;
}

// Statement ----------------------------------------------------------------

// Move the statement's current position.
int MoveStatementAdapter(history::AndroidStatement* statement,
                         int current_pos,
                         int destination,
                         history::AndroidProviderBackend* backend) {
  DCHECK_LE(-1, current_pos);
  DCHECK_LE(-1, destination);

  int cur = current_pos;
  if (current_pos > destination) {
    statement->statement()->Reset(false);
    cur = -1;
  }
  for (; cur < destination; ++cur) {
    if (!statement->statement()->Step())
      break;
  }

  return cur;
}

// CloseStatementTask delete the passed |statement| in the DB thread (or in the
// UI thread if the HistoryBackend is destroyed before the task is executed).
class CloseStatementTask : public history::HistoryDBTask {
 public:
  // Close the given statement. The ownership is transfered.
  explicit CloseStatementTask(history::AndroidStatement* statement)
      : statement_(statement) {
    DCHECK(statement_);
  }

  ~CloseStatementTask() override { delete statement_; }

  // Returns the cancelable task tracker to use for this task. The task owns it,
  // so it can never be cancelled. This is required due to be compatible with
  // HistoryService::ScheduleDBTask() interface.
  base::CancelableTaskTracker* tracker() { return &tracker_; }

 private:
  // history::HistoryDBTask implementation.
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    delete statement_;
    statement_ = nullptr;
    return true;
  }

  void DoneRunOnMainThread() override {}

  history::AndroidStatement* statement_;
  base::CancelableTaskTracker tracker_;
};

// Search terms -------------------------------------------------------------

// Inserts the given values and returns the SearchTermID of the inserted row.
history::SearchTermID InsertSearchTermAdapter(
    const history::SearchRow& row,
    history::AndroidProviderBackend* backend) {
  return backend->InsertSearchTerm(row);
}

// Returns the number of row updated by the update query.
//
// |row| is the value to update.
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for the WHERE clause.
int UpdateSearchTermsAdapter(const history::SearchRow& row,
                             const std::string& selection,
                             const std::vector<base::string16> selection_args,
                             history::AndroidProviderBackend* backend) {
  int count = 0;
  backend->UpdateSearchTerms(row, selection, selection_args, &count);
  return count;
}

// Deletes the matched rows and returns the number of deleted rows.
//
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for WHERE clause.
//
// If |selection| is empty all search terms will be deleted.
int DeleteSearchTermsAdapter(const std::string& selection,
                             const std::vector<base::string16> selection_args,
                             history::AndroidProviderBackend* backend) {
  int count = 0;
  backend->DeleteSearchTerms(selection, selection_args, &count);
  return count;
}

// Returns the result of the given query.
//
// |projections| specifies the result columns.
// |selection| is the SQL WHERE clause without 'WHERE'.
// |selection_args| is the arguments for WHERE clause.
// |sort_order| is the SQL ORDER clause.
history::AndroidStatement* QuerySearchTermsAdapter(
    const std::vector<history::SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    history::AndroidProviderBackend* backend) {
  return backend->QuerySearchTerms(projections, selection, selection_args,
                                   sort_order);
}

}  // namespace

AndroidHistoryProviderService::AndroidHistoryProviderService(Profile* profile)
    : profile_(profile) {
}

AndroidHistoryProviderService::~AndroidHistoryProviderService() {
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::QueryHistoryAndBookmarks(
    const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    const QueryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(nullptr);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&QueryHistoryAndBookmarksAdapter, projections, selection,
                     selection_args, sort_order),
          callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::UpdateHistoryAndBookmarks(
    const history::HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const UpdateCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(base::Bind(&UpdateHistoryAndBookmarksAdapter,
                                           row, selection, selection_args),
                                callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::DeleteHistoryAndBookmarks(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const DeleteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(base::Bind(&DeleteHistoryAndBookmarksAdapter,
                                           selection, selection_args),
                                callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::InsertHistoryAndBookmark(
    const history::HistoryAndBookmarkRow& values,
    const InsertCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&InsertHistoryAndBookmarkAdapter, values), callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::DeleteHistory(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const DeleteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&DeleteHistoryAdapter, selection, selection_args),
          callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::MoveStatement(
    history::AndroidStatement* statement,
    int current_pos,
    int destination,
    const MoveStatementCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(current_pos);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(base::Bind(&MoveStatementAdapter, statement,
                                           current_pos, destination),
                                callback),
      tracker);
}

void AndroidHistoryProviderService::CloseStatement(
    history::AndroidStatement* statement) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    delete statement;
    return;
  }
  std::unique_ptr<CloseStatementTask> task(new CloseStatementTask(statement));
  base::CancelableTaskTracker* tracker = task->tracker();
  hs->ScheduleDBTask(FROM_HERE, std::move(task), tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::InsertSearchTerm(
    const history::SearchRow& row,
    const InsertCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(base::Bind(&InsertSearchTermAdapter, row),
                                callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::UpdateSearchTerms(
    const history::SearchRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const UpdateCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&UpdateSearchTermsAdapter, row, selection, selection_args),
          callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::DeleteSearchTerms(
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const DeleteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(0);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&DeleteSearchTermsAdapter, selection, selection_args),
          callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::QuerySearchTerms(
    const std::vector<history::SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    const QueryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!hs) {
    callback.Run(nullptr);
    return base::CancelableTaskTracker::kBadTaskId;
  }
  return hs->ScheduleDBTask(
      FROM_HERE,
      CreateAndroidProviderTask(
          base::Bind(&QuerySearchTermsAdapter, projections, selection,
                     selection_args, sort_order),
          callback),
      tracker);
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::GetLargestRawFaviconForID(
    favicon_base::FaviconID favicon_id,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  favicon::FaviconService* fs = FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(fs);
  return fs->GetLargestRawFaviconForID(favicon_id, std::move(callback),
                                       tracker);
}
