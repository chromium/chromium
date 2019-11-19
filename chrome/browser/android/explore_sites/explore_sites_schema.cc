// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_schema.h"

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace explore_sites {

constexpr int ExploreSitesSchema::kCurrentVersion;
constexpr int ExploreSitesSchema::kCompatibleVersion;

const char ExploreSitesSchema::kCurrentCatalogKey[] = "current_catalog";
const char ExploreSitesSchema::kDownloadingCatalogKey[] = "downloading_catalog";

// Schema versions changelog:
// * 1: Initial version.
// * 2: Version with ntp shown count.

namespace {
static const char kCategoriesTableCreationSql[] =
    "CREATE TABLE IF NOT EXISTS categories ("
    "category_id INTEGER PRIMARY KEY AUTOINCREMENT, "  // for local use only,
                                                       // not known by server.
                                                       // ID is *not* retained
                                                       // across catalog
                                                       // updates.
    "version_token TEXT NOT NULL, "  // matches an entry in the meta table:
                                     // ‘current_catalog’ or
                                     // ‘downloading_catalog’.
    "type INTEGER NOT NULL, "
    "label TEXT NOT NULL, "
    "image BLOB, "  // can be NULL if no image is available, but must be
                    // populated for use on the NTP.
    "ntp_click_count INTEGER NOT NULL DEFAULT 0, "
    "esp_site_click_count INTEGER NOT NULL DEFAULT 0,"
    "ntp_shown_count INTEGER NOT NULL DEFAULT 0);";  // number of times the
                                                     // category was shown on
                                                     // the ntp.

bool CreateCategoriesTable(sql::Database* db) {
  return db->Execute(kCategoriesTableCreationSql);
}

static const char kSitesTableCreationSql[] =
    "CREATE TABLE IF NOT EXISTS sites ("
    "site_id INTEGER PRIMARY KEY AUTOINCREMENT, "  // locally generated. Same
                                                   // caveats as |category_id|.
    "url TEXT NOT NULL, "
    "category_id INTEGER NOT NULL, "  // A row from the categories table.
    "title TEXT NOT NULL, "
    // The favicon of the website encoded as a WebP.
    "favicon BLOB, "
    "click_count INTEGER NOT NULL DEFAULT 0, "
    "removed BOOLEAN NOT NULL default FALSE); ";

bool CreateSitesTable(sql::Database* db) {
  return db->Execute(kSitesTableCreationSql);
}

static const char kActivityTableCreationSql[] =
    "CREATE TABLE IF NOT EXISTS activity ("
    "time INTEGER NOT NULL,"           // stored as unix timestamp
    "category_type INTEGER NOT NULL,"  // matches the type of a category
    "url TEXT NOT NULL);";             // matches the url of a site

bool CreateActivityTable(sql::Database* db) {
  return db->Execute(kActivityTableCreationSql);
}

static const char kSiteBlacklistTableCreationSql[] =
    "CREATE TABLE IF NOT EXISTS site_blacklist ("
    "url TEXT NOT NULL UNIQUE, "
    "date_removed INTEGER NOT NULL);";  // stored as unix timestamp

bool CreateSiteBlacklistTable(sql::Database* db) {
  return db->Execute(kSiteBlacklistTableCreationSql);
}

bool CreateLatestSchema(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!CreateCategoriesTable(db) || !CreateSitesTable(db) ||
      !CreateSiteBlacklistTable(db) || !CreateActivityTable(db)) {
    return false;
  }

  return transaction.Commit();
}

int MigrateFrom1To2(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 2 adds a new column.
  const int target_version = 2;
  static const char k1To2Sql[] =
      "ALTER TABLE categories ADD COLUMN ntp_shown_count INTEGER NOT NULL "
      "DEFAULT 0;";
  sql::Transaction transaction(db);
  if (transaction.Begin() && db->Execute(k1To2Sql) && transaction.Commit()) {
    meta_table->SetVersionNumber(target_version);
    return target_version;
  }
  return 1;
}

}  // namespace

// static
bool ExploreSitesSchema::InitMetaTable(sql::Database* db,
                                       sql::MetaTable* meta_table) {
  DCHECK(meta_table);
  return meta_table->Init(db, kCurrentVersion, kCompatibleVersion);
}

// static
bool ExploreSitesSchema::CreateOrUpgradeIfNeeded(sql::Database* db) {
  DCHECK_GE(kCurrentVersion, kCompatibleVersion);
  if (!db)
    return false;

  sql::MetaTable meta_table;
  if (!InitMetaTable(db, &meta_table))
    return false;

  const int compatible_version = meta_table.GetCompatibleVersionNumber();
  int current_version = meta_table.GetVersionNumber();
  // sql::MetaTable docs say that an error returns 0 for the version number,
  // and DCHECKs in that class prevent it from being the actual version number
  // stored.
  const int kInvalidVersion = 0;
  if (current_version == kInvalidVersion ||
      compatible_version == kInvalidVersion)
    return false;
  DCHECK_GE(current_version, compatible_version);

  // Stored database version is newer and incompatible with the current running
  // code (Chrome was downgraded). The DB will never work until Chrome is
  // re-upgraded.
  if (compatible_version > kCurrentVersion)
    return false;

  // Database is already at the latest version or has just been created. Create
  // any missing tables and return.
  if (current_version == kCurrentVersion)
    return CreateLatestSchema(db);

  // Versions 0 and below are unexpected.
  if (current_version <= 0)
    return false;

  // NOTE: Insert schema upgrade scripts here when required.
  if (current_version == 1)
    current_version = MigrateFrom1To2(db, &meta_table);

  return current_version == kCurrentVersion;
}

// static
std::pair<std::string, std::string> ExploreSitesSchema::GetVersionTokens(
    sql::Database* db) {
  sql::MetaTable meta_table;
  if (!InitMetaTable(db, &meta_table))
    return std::make_pair("", "");

  std::string current_version_token, downloading_version_token;

  meta_table.GetValue(kCurrentCatalogKey, &current_version_token);
  meta_table.GetValue(kDownloadingCatalogKey, &downloading_version_token);

  return std::make_pair(current_version_token, downloading_version_token);
}

}  // namespace explore_sites
