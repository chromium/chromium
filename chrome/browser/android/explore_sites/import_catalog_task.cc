// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/import_catalog_task.h"

#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kDeleteExistingCategorySql[] =
    "DELETE FROM categories WHERE version_token = ?";

static const char kInsertCategorySql[] = R"(INSERT INTO categories
(version_token, type, label, image)
VALUES
(?, ?, ?, ?);)";

static const char kDeleteExistingSiteSql[] = R"(DELETE FROM sites
WHERE (
 SELECT COUNT(1) FROM categories
 WHERE category_id = sites.category_id AND categories.version_token = ?) > 0)";

static const char kInsertSiteSql[] = R"(INSERT INTO sites
(url, category_id, title, favicon)
VALUES
(?, ?, ?, ?);)";

}  // namespace

bool ImportCatalogSync(std::string version_token,
                       std::unique_ptr<Catalog> catalog_proto,
                       sql::Database* db) {
  if (!db || !catalog_proto || version_token.empty())
    return false;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  sql::MetaTable meta_table;
  if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
    return false;

  // If we are downloading a catalog that is the same version as the one
  // currently in use, don't change it.  This is an error, should have been
  // caught before we got here.
  std::string current_version_token;
  if (meta_table.GetValue("current_catalog", &current_version_token) &&
      current_version_token == version_token) {
    return false;
  }

  // In case we get a duplicate timestamp for the downloading catalog, remove
  // the catalog with the timestamp that matches the catalog we are importing.
  sql::Statement site_clear_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteExistingSiteSql));
  site_clear_statement.BindString(0, version_token);
  site_clear_statement.Run();

  sql::Statement category_clear_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteExistingCategorySql));
  category_clear_statement.BindString(0, version_token);
  category_clear_statement.Run();

  // Update the downloading catalog version number to match what we are
  // importing.
  if (!meta_table.SetValue("downloading_catalog", version_token))
    return false;

  // Then insert each category.
  for (auto category : catalog_proto->categories()) {
    sql::Statement category_statement(
        db->GetCachedStatement(SQL_FROM_HERE, kInsertCategorySql));

    int col = 0;
    category_statement.BindString(col++, version_token);
    category_statement.BindInt(col++, static_cast<int>(category.type()));
    category_statement.BindString(col++, category.localized_title());
    category_statement.BindBlob(col++, category.icon().data(),
                                category.icon().length());

    category_statement.Run();

    int64_t category_id = db->GetLastInsertRowId();

    for (auto site : category.sites()) {
      sql::Statement site_statement(
          db->GetCachedStatement(SQL_FROM_HERE, kInsertSiteSql));

      col = 0;
      site_statement.BindString(col++, site.site_url());
      site_statement.BindInt64(col++, category_id);
      site_statement.BindString(col++, site.title());
      site_statement.BindBlob(col++, site.icon().data(), site.icon().length());

      site_statement.Run();
    }
  }

  return transaction.Commit();
}

ImportCatalogTask::ImportCatalogTask(ExploreSitesStore* store,
                                     std::string version_token,
                                     std::unique_ptr<Catalog> catalog_proto,
                                     BooleanCallback callback)
    : store_(store),
      version_token_(version_token),
      catalog_proto_(std::move(catalog_proto)),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

ImportCatalogTask::~ImportCatalogTask() = default;

void ImportCatalogTask::Run() {
  store_->Execute(base::BindOnce(&ImportCatalogSync, version_token_,
                                 std::move(catalog_proto_)),
                  base::BindOnce(&ImportCatalogTask::FinishedExecuting,
                                 weak_ptr_factory_.GetWeakPtr()),
                  false);
}

void ImportCatalogTask::FinishedExecuting(bool result) {
  complete_ = true;
  result_ = result;
  TaskComplete();
  DVLOG(1) << "Finished importing the catalog, result: " << result;
  std::move(callback_).Run(result);
}

}  // namespace explore_sites
