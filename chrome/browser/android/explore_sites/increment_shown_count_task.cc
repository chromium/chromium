// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/increment_shown_count_task.h"

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kIncrementShownCountSql[] =
    "UPDATE categories "
    "SET ntp_shown_count = ntp_shown_count + 1 WHERE category_id = ?";

}  // namespace

bool IncrementShownCountTaskSync(int category_id, sql::Database* db) {
  if (!db)
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  sql::MetaTable meta_table;
  if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
    return false;

  sql::Statement increment_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kIncrementShownCountSql));
  increment_statement.BindInt(0, category_id);
  increment_statement.Run();

  return transaction.Commit();
}

IncrementShownCountTask::IncrementShownCountTask(ExploreSitesStore* store,
                                                 int category_id)
    : store_(store), category_id_(category_id) {}

IncrementShownCountTask::~IncrementShownCountTask() = default;

void IncrementShownCountTask::Run() {
  store_->Execute(base::BindOnce(&IncrementShownCountTaskSync, category_id_),
                  base::BindOnce(&IncrementShownCountTask::FinishedExecuting,
                                 weak_factory_.GetWeakPtr()),
                  false);
}

void IncrementShownCountTask::FinishedExecuting(bool result) {
  complete_ = true;
  result_ = result;
  TaskComplete();
}

}  // namespace explore_sites
