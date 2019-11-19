// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/clear_catalog_task.h"

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
ClearCatalogTask::ClearCatalogTask(ExploreSitesStore* store,
                                   BooleanCallback callback)
    : store_(store), callback_(std::move(callback)) {}

ClearCatalogTask::~ClearCatalogTask() = default;

void ClearCatalogTask::Run() {
  store_->Execute(
      base::BindOnce([](sql::Database* db) {
        sql::MetaTable meta_table;
        if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
          return false;

        meta_table.DeleteKey(ExploreSitesSchema::kCurrentCatalogKey);
        meta_table.DeleteKey(ExploreSitesSchema::kDownloadingCatalogKey);

        sql::Statement site_clear_statement(
            db->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM sites"));
        if (!site_clear_statement.Run())
          return false;

        sql::Statement category_clear_statement(
            db->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM categories"));
        if (!category_clear_statement.Run())
          return false;

        return true;
      }),
      base::BindOnce(&ClearCatalogTask::DoneExecuting,
                     weak_factory_.GetWeakPtr()),
      false);
}

void ClearCatalogTask::DoneExecuting(bool result) {
  std::move(callback_).Run(result);
  TaskComplete();
}

}  // namespace explore_sites
