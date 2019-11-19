// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SCHEMA_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SCHEMA_H_

#include <string>
#include <tuple>

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace explore_sites {

// Maintains the schema of the "Explore Sites" database, ensuring creation and
// upgrades from any and all previous database versions to the latest.
class ExploreSitesSchema {
 public:
  static constexpr int kCurrentVersion = 2;
  static constexpr int kCompatibleVersion = 1;

  static const char kCurrentCatalogKey[];
  static const char kDownloadingCatalogKey[];

  // Initializes the given meta table using the appropriate versions.
  static bool InitMetaTable(sql::Database* db, sql::MetaTable* meta_table);

  // Creates or upgrade the database schema as needed from information stored in
  // a metadata table. Returns |true| if the database is ready to be used,
  // |false| if creation or upgrades failed.
  static bool CreateOrUpgradeIfNeeded(sql::Database* db);

  // Returns a pair representing the current and downloading version tokens.
  static std::pair<std::string, std::string> GetVersionTokens(
      sql::Database* db);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SCHEMA_H_
