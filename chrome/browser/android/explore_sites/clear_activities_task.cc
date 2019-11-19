// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/clear_activities_task.h"

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace explore_sites {

namespace {

static const char kClearActivitiesSql[] =
    "DELETE FROM activity WHERE time >= ? AND time < ?";

bool ClearActivitiesTaskSync(base::Time begin,
                             base::Time end,
                             sql::Database* db) {
  if (!db)
    return false;

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kClearActivitiesSql));
  statement.BindInt64(0, offline_pages::store_utils::ToDatabaseTime(begin));
  statement.BindInt64(1, offline_pages::store_utils::ToDatabaseTime(end));
  return statement.Run();
}

}  // namespace

ClearActivitiesTask::ClearActivitiesTask(ExploreSitesStore* store,
                                         base::Time begin,
                                         base::Time end,
                                         BooleanCallback callback)
    : store_(store), begin_(begin), end_(end), callback_(std::move(callback)) {}

ClearActivitiesTask::~ClearActivitiesTask() = default;

void ClearActivitiesTask::Run() {
  store_->Execute(base::BindOnce(&ClearActivitiesTaskSync, begin_, end_),
                  base::BindOnce(&ClearActivitiesTask::DoneExecuting,
                                 weak_factory_.GetWeakPtr()),
                  false);
}

void ClearActivitiesTask::DoneExecuting(bool result) {
  std::move(callback_).Run(result);
  TaskComplete();
}

}  // namespace explore_sites
