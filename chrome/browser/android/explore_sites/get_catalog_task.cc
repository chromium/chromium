// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/get_catalog_task.h"

#include "base/bind.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace explore_sites {
namespace {

static const char kSelectCategorySql[] = R"(SELECT
category_id, type, label, ntp_shown_count, activityCount.count
FROM categories
LEFT JOIN (SELECT COUNT(url) as count, category_type
FROM activity GROUP BY category_type) AS activityCount
ON categories.type = activityCount.category_type
WHERE version_token = ?
ORDER BY category_id ASC;)";

static const char kSelectSiteSql[] =
    R"(SELECT site_id, sites.url, title,
    site_blacklist.url IS NOT NULL as blacklisted
FROM sites
LEFT JOIN site_blacklist ON (sites.url = site_blacklist.url)
WHERE category_id = ? ;)";

const char kDeleteSiteSql[] = R"(DELETE FROM sites
WHERE category_id NOT IN
(SELECT category_id FROM categories WHERE version_token = ?);)";

const char kDeleteCategorySql[] =
    "DELETE FROM categories WHERE version_token <> ?;";

}  // namespace

// |current_version_token| represents the version in "current_catalog" key of
// the meta table.  We check whether there exists a "downloading_catalog", and
// if there doesn't, just return the |current_version_token|.  If there is, set
// the current == the downloading catalog and return the downloading (aka the
// new current.);
std::string UpdateCurrentCatalogIfNewer(sql::MetaTable* meta_table,
                                        std::string current_version_token) {
  DCHECK(meta_table);
  std::string downloading_version_token;
  // See if there is a downloading catalog.
  if (!meta_table->GetValue(ExploreSitesSchema::kDownloadingCatalogKey,
                            &downloading_version_token)) {
    // No downloading catalog means no change required.
    return current_version_token;
  }

  // Update the current version.
  current_version_token = downloading_version_token;
  if (!meta_table->SetValue(ExploreSitesSchema::kCurrentCatalogKey,
                            current_version_token))
    return "";
  meta_table->DeleteKey(ExploreSitesSchema::kDownloadingCatalogKey);

  return downloading_version_token;
}

void RemoveOutdatedCatalogEntries(sql::Database* db,
                                  std::string version_token) {
  // Deletes sites and categories with a version that doesn't match
  // |version_token|.
  sql::Statement delete_sites(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSiteSql));
  delete_sites.BindString(0, version_token);
  delete_sites.Run();

  sql::Statement delete_categories(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteCategorySql));
  delete_categories.BindString(0, version_token);
  delete_categories.Run();
}

std::pair<GetCatalogStatus, std::unique_ptr<GetCatalogTask::CategoryList>>
GetCatalogSync(bool update_current, sql::Database* db) {
  DCHECK(db);
  sql::MetaTable meta_table;
  if (!ExploreSitesSchema::InitMetaTable(db, &meta_table))
    return std::make_pair(GetCatalogStatus::kFailed, nullptr);

  // If we are downloading a catalog that is the same version as the one
  // currently in use, don't change it.  This is an error, should have been
  // caught before we got here.
  std::string catalog_version_token;
  if (!meta_table.GetValue(ExploreSitesSchema::kCurrentCatalogKey,
                           &catalog_version_token) ||
      catalog_version_token.empty()) {
    DVLOG(1)
        << "Didn't find current catalog value. Attempting to use downloading.";
    // If there is no current catalog, use downloading catalog and mark it as
    // current.  If there is no downloading catalog, return no catalog.
    meta_table.GetValue(ExploreSitesSchema::kDownloadingCatalogKey,
                        &catalog_version_token);
    if (catalog_version_token.empty())
      return std::make_pair(GetCatalogStatus::kNoCatalog, nullptr);

    update_current = true;
  }

  if (update_current) {
    DVLOG(1) << "Updating current catalog from " << catalog_version_token;
    sql::Transaction transaction(db);
    transaction.Begin();
    catalog_version_token =
        UpdateCurrentCatalogIfNewer(&meta_table, catalog_version_token);
    if (catalog_version_token == "")
      return std::make_pair(GetCatalogStatus::kFailed, nullptr);

    RemoveOutdatedCatalogEntries(db, catalog_version_token);

    if (!transaction.Commit())
      return std::make_pair(GetCatalogStatus::kFailed, nullptr);
  }

  DVLOG(1) << "Done updating. Catalog to use: " << catalog_version_token;

  sql::Statement category_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectCategorySql));
  category_statement.BindString(0, catalog_version_token);

  auto result = std::make_unique<GetCatalogTask::CategoryList>();
  while (category_statement.Step()) {
    result->emplace_back(category_statement.ColumnInt(0),  // category_id
                         catalog_version_token,
                         category_statement.ColumnInt(1),     // type
                         category_statement.ColumnString(2),  // label
                         category_statement.ColumnInt(3),     // ntp_shown_count
                         category_statement.ColumnInt(4));  // interaction_count
  }
  if (!category_statement.Succeeded())
    return std::make_pair(GetCatalogStatus::kFailed, nullptr);

  bool found_empty_category = false;
  for (auto& category : *result) {
    sql::Statement site_statement(
        db->GetCachedStatement(SQL_FROM_HERE, kSelectSiteSql));
    site_statement.BindInt64(0, category.category_id);

    while (site_statement.Step()) {
      category.sites.emplace_back(
          site_statement.ColumnInt(0),  // site_id
          category.category_id,
          GURL(site_statement.ColumnString(1)),  // url
          site_statement.ColumnString(2),        // title
          site_statement.ColumnBool(3));         // is_blacklisted
    }
    if (!site_statement.Succeeded())
      return std::make_pair(GetCatalogStatus::kFailed, nullptr);

    if (category.sites.empty())
      found_empty_category = true;
  }

  // Remove any categories with no sites.
  if (found_empty_category) {
    auto new_result = std::make_unique<GetCatalogTask::CategoryList>();
    for (auto& category : *result) {
      // Move all categories with sites into the new result
      if (category.sites.size() > 0)
        new_result->emplace_back(std::move(category));
    }
    result = std::move(new_result);
  }

  return std::make_pair(GetCatalogStatus::kSuccess, std::move(result));
}

GetCatalogTask::GetCatalogTask(ExploreSitesStore* store,
                               bool update_current,
                               CatalogCallback callback)
    : store_(store),
      update_current_(update_current),
      callback_(std::move(callback)) {}

GetCatalogTask::~GetCatalogTask() = default;

void GetCatalogTask::Run() {
  store_->Execute(base::BindOnce(&GetCatalogSync, update_current_),
                  base::BindOnce(&GetCatalogTask::FinishedExecuting,
                                 weak_ptr_factory_.GetWeakPtr()),
                  std::make_pair(GetCatalogStatus::kFailed,
                                 std::unique_ptr<CategoryList>()));
}

void GetCatalogTask::FinishedExecuting(
    std::pair<GetCatalogStatus, std::unique_ptr<CategoryList>> result) {
  TaskComplete();
  DVLOG(1) << "Finished getting the catalog, result: "
           << static_cast<int>(std::get<0>(result));
  std::move(callback_).Run(std::get<0>(result), std::move(std::get<1>(result)));
}

}  // namespace explore_sites
