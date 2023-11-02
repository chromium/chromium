// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_schema.h"

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace explore_sites {
namespace {
std::vector<std::string> TableColumns(sql::Database* db,
                                      const std::string table_name) {
  std::vector<std::string> columns;
  std::string sql = "PRAGMA TABLE_INFO(" + table_name + ")";
  sql::Statement table_info(db->GetUniqueStatement(sql.c_str()));
  while (table_info.Step())
    columns.push_back(table_info.ColumnString(1));
  return columns;
}

struct Table {
  std::string ToString() const {
    std::ostringstream ss;
    ss << "-- TABLE " << name << " --\n";
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
      ss << "--- ROW " << row_index << " ---\n";
      const std::vector<std::string>& row = rows[row_index];
      for (size_t i = 0; i < row.size(); ++i) {
        ss << column_names[i] << ": " << base::GetQuotedJSONString(row[i])
           << '\n';
      }
    }
    return ss.str();
  }

  std::string name;
  std::vector<std::string> column_names;
  // List of all values. Has size [row_count][column_count].
  std::vector<std::vector<std::string>> rows;
};

struct DatabaseTables {
  std::string ToString() {
    std::ostringstream ss;
    for (auto i = tables.begin(); i != tables.end(); ++i)
      ss << i->second.ToString();
    return ss.str();
  }
  std::map<std::string, Table> tables;
};

Table ReadTable(sql::Database* db, const std::string table_name) {
  Table table;
  table.name = table_name;
  table.column_names = TableColumns(db, table_name);
  std::string sql = "SELECT * FROM " + table_name;
  sql::Statement all_data(db->GetUniqueStatement(sql.c_str()));
  while (all_data.Step()) {
    std::vector<std::string> row;
    for (size_t i = 0; i < table.column_names.size(); ++i) {
      row.push_back(all_data.ColumnString(i));
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

// Returns all tables in |db|, except the 'meta' table and tables that begin
// with 'sqlite_'. We don't test the 'meta' table directly in this file, but
// instead use the MetaTable class.
DatabaseTables ReadTables(sql::Database* db) {
  DatabaseTables database_tables;
  std::stringstream ss;
  sql::Statement table_names(db->GetUniqueStatement(
      "SELECT name FROM sqlite_schema WHERE type='table'"));
  while (table_names.Step()) {
    const std::string table_name = table_names.ColumnString(0);
    if (table_name == "meta" || base::StartsWith(table_name, "sqlite_"))
      continue;
    database_tables.tables[table_name] = ReadTable(db, table_name);
  }
  return database_tables;
}

// Returns the SQL that defines a table.
std::string TableSql(sql::Database* db, const std::string& table_name) {
  DatabaseTables database_tables;
  std::stringstream ss;
  sql::Statement table_sql(db->GetUniqueStatement(
      "SELECT sql FROM sqlite_schema WHERE type='table' AND name=?"));
  table_sql.BindString(0, table_name);
  if (!table_sql.Step())
    return std::string();
  // Try to normalize the SQL, since we use this to compare schemas.
  std::string sql =
      base::CollapseWhitespaceASCII(table_sql.ColumnString(0), true);
  base::ReplaceSubstringsAfterOffset(&sql, 0, ", ", ",");
  base::ReplaceSubstringsAfterOffset(&sql, 0, ",", ",\n");
  base::ReplaceSubstringsAfterOffset(&sql, 0, " (", "(");
  base::ReplaceSubstringsAfterOffset(&sql, 0, "\"", "");
  return sql;
}

std::string ReadSchemaFile(const std::string& file_name) {
  std::string data;
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.AppendASCII("components/test/data/explore_sites/version_schemas/")
             .AppendASCII(file_name);
  CHECK(base::ReadFileToString(path, &data)) << path;
  return data;
}

std::unique_ptr<sql::Database> CreateTablesWithSampleRows(int version) {
  auto db = std::make_unique<sql::Database>();
  CHECK(db->OpenInMemory());
  // Write a meta table. v*.sql overwrites version and last_compatible_version.
  sql::MetaTable meta_table;
  CHECK(meta_table.Init(db.get(), 1, 1));

  const std::string schema = ReadSchemaFile(
      base::StrCat({"v", base::NumberToString(version), ".sql"}));
  CHECK(db->Execute(schema.c_str()));
  return db;
}

void ExpectDbIsCurrent(sql::Database* db) {
  // Check the meta table.
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db, 1, 1));
  EXPECT_EQ(ExploreSitesSchema::kCurrentVersion, meta_table.GetVersionNumber());
  EXPECT_EQ(ExploreSitesSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());

  std::unique_ptr<sql::Database> golden_db =
      CreateTablesWithSampleRows(ExploreSitesSchema::kCurrentVersion);

  // Check that database schema is current.
  for (auto name_and_table : ReadTables(db).tables) {
    const std::string golden_sql =
        TableSql(golden_db.get(), name_and_table.first);
    const std::string real_sql = TableSql(db, name_and_table.first);
    EXPECT_EQ(golden_sql, real_sql);
  }
}
}  // namespace

class ExploreSitesSchemaTest : public testing::Test {
 public:
  ExploreSitesSchemaTest() = default;
  ~ExploreSitesSchemaTest() override = default;

  void SetUp() override {
    db_ = std::make_unique<sql::Database>();
    ASSERT_TRUE(db_->OpenInMemory());
    ASSERT_FALSE(sql::MetaTable::DoesTableExist(db_.get()));
  }

  void CheckTablesExistence() {
    EXPECT_TRUE(db_->DoesTableExist("sites"));
    EXPECT_TRUE(db_->DoesTableExist("categories"));
    EXPECT_TRUE(db_->DoesTableExist("site_blocklist"));
    EXPECT_TRUE(db_->DoesTableExist("activity"));
  }

 protected:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<ExploreSitesSchema> schema_;
};

TEST_F(ExploreSitesSchemaTest, TestSchemaCreationFromNothing) {
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db_.get(), std::numeric_limits<int>::max(),
                              std::numeric_limits<int>::max()));
  EXPECT_EQ(ExploreSitesSchema::kCurrentVersion, meta_table.GetVersionNumber());
  EXPECT_EQ(ExploreSitesSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());
}

TEST_F(ExploreSitesSchemaTest, TestMissingTablesAreCreatedAtLatestVersion) {
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db_.get(), ExploreSitesSchema::kCurrentVersion,
                              ExploreSitesSchema::kCompatibleVersion));
  EXPECT_EQ(ExploreSitesSchema::kCurrentVersion, meta_table.GetVersionNumber());
  EXPECT_EQ(ExploreSitesSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());

  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
}

TEST_F(ExploreSitesSchemaTest, TestMissingTablesAreRecreated) {
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE sites"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE categories"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE site_blocklist"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();

  EXPECT_TRUE(db_->Execute("DROP TABLE activity"));
  EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db_.get()));
  CheckTablesExistence();
}

TEST_F(ExploreSitesSchemaTest, TestMigrate) {
  for (int i = 0; i <= ExploreSitesSchema::kCurrentVersion; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing migration from version " << i);
    std::unique_ptr<sql::Database> db;
    // When i==0, start from an empty state.
    const int version = i > 0 ? i : ExploreSitesSchema::kCurrentVersion;
    if (i > 0) {
      db = CreateTablesWithSampleRows(i);
      // Executes the migration.
      EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db.get()));
    } else {
      db = std::make_unique<sql::Database>();
      ASSERT_TRUE(db->OpenInMemory());
      // Creation from scratch.
      EXPECT_TRUE(ExploreSitesSchema::CreateOrUpgradeIfNeeded(db.get()));
      // Tables are already created, this will just insert rows.
      const std::string schema = ReadSchemaFile(
          base::StrCat({"v", base::NumberToString(version), ".sql"}));
      ASSERT_TRUE(db->Execute(schema.c_str()));
    }

    // Check schema.
    ExpectDbIsCurrent(db.get());

    // Check the database contents.
    std::string expected_data = ReadSchemaFile(
        base::StrCat({"v", base::NumberToString(version), ".data"}));
    EXPECT_EQ(expected_data, ReadTables(db.get()).ToString());
  }
}

}  // namespace explore_sites
