// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/block_site_task.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/core/offline_clock.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kBlockSiteSql[] = R"(INSERT INTO site_blocklist
(url, date_removed)
VALUES
(?, ?);)";

static const char kClearSiteFromActivitySql[] =
    "DELETE FROM activity WHERE url = ?;";
}  // namespace

bool BlockSiteTaskSync(std::string url, sql::Database* db) {
  if (!db || url.empty())
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  sql::MetaTable meta_table;
  if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
    return false;

  // Get current time as a unix time.
  base::Time time_now = offline_pages::OfflineTimeNow();
  time_t unix_time = time_now.ToTimeT();

  // Then insert the URL.
  sql::Statement block_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kBlockSiteSql));

  int col = 0;
  block_statement.BindString(col++, url);
  block_statement.BindInt64(col++, unix_time);
  block_statement.Run();

  // Then clear all matching activity.
  sql::Statement clear_activity_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kClearSiteFromActivitySql));

  clear_activity_statement.BindString(0, url);
  clear_activity_statement.Run();

  return transaction.Commit();
}

BlockSiteTask::BlockSiteTask(ExploreSitesStore* store, std::string url)
    : store_(store), url_(url) {}

BlockSiteTask::~BlockSiteTask() = default;

void BlockSiteTask::Run() {
  store_->Execute(base::BindOnce(&BlockSiteTaskSync, url_),
                  base::BindOnce(&BlockSiteTask::FinishedExecuting,
                                 weak_ptr_factory_.GetWeakPtr()),
                  false);
}

void BlockSiteTask::FinishedExecuting(bool result) {
  complete_ = true;
  result_ = result;
  TaskComplete();
  DVLOG(1) << "Finished adding a site to the blocklist, result: " << result;
}

}  // namespace explore_sites
