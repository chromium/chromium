// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/record_site_click_task.h"

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/core/offline_clock.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kRecordSiteSql[] = R"(INSERT INTO activity
(time, url, category_type)
VALUES
(?, ?, ?);)";

static const char kDeleteLeastRecentActivitySql[] = R"(DELETE FROM activity
WHERE rowid NOT IN (SELECT rowid FROM activity ORDER BY time DESC LIMIT 200);)";

bool RecordSiteClickTaskSync(std::string url,
                             int category_type,
                             sql::Database* db) {
  if (!db || url.empty())
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Get current time as a unit time.
  base::Time time_now = offline_pages::OfflineTimeNow();
  time_t unix_time = time_now.ToTimeT();

  sql::Statement insert_activity_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kRecordSiteSql));
  int col = 0;
  insert_activity_statement.BindInt64(col++, unix_time);
  insert_activity_statement.BindString(col++, url);
  insert_activity_statement.BindInt(col++, category_type);
  insert_activity_statement.Run();

  // Remove least recent activities if table greater than the limit.
  sql::Statement remove_activity_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteLeastRecentActivitySql));
  remove_activity_statement.Run();
  return transaction.Commit();
}
}  // namespace

RecordSiteClickTask::RecordSiteClickTask(ExploreSitesStore* store,
                                         std::string url,
                                         int category_type)
    : store_(store), url_(url), category_type_(category_type) {}

RecordSiteClickTask::~RecordSiteClickTask() = default;

void RecordSiteClickTask::Run() {
  store_->Execute(
      base::BindOnce(&RecordSiteClickTaskSync, url_, category_type_),
      base::BindOnce(&RecordSiteClickTask::FinishedExecuting,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void RecordSiteClickTask::FinishedExecuting(bool result) {
  complete_ = true;
  result_ = result;
  TaskComplete();
}
}  // namespace explore_sites
