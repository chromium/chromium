// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_database_migrator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using internal::DIPSDatabaseMigrator;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

class DIPSDatabaseMigrationTest : public testing::Test {
 protected:
  base::FilePath db_path() { return db_path_; }

  [[nodiscard]] AssertionResult LoadDatabase(const char* file_name) {
    base::FilePath root;
    if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root)) {
      return AssertionFailure() << "Could not get test data directory root";
    }
    base::FilePath file_path = root.AppendASCII("chrome")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("dips")
                                   .AppendASCII(file_name);
    if (!base::PathExists(file_path)) {
      return AssertionFailure() << file_path << " does not exist";
    }
    if (!sql::test::CreateDatabaseFromSQL(db_path(), file_path)) {
      return AssertionFailure() << "Could not create database at " << db_path();
    }
    return AssertionSuccess();
  }

  int GetDatabaseVersion(sql::Database* db) {
    sql::Statement kGetVersionSql(
        db->GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
    if (!kGetVersionSql.Step()) {
      return 0;
    }
    return kGetVersionSql.ColumnInt(0);
  }

  int GetDatabaseLastCompatibleVersion(sql::Database* db) {
    sql::Statement kGetLastCompatibleVersionSql(db->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='last_compatible_version'"));
    if (!kGetLastCompatibleVersionSql.Step()) {
      return 0;
    }
    return kGetLastCompatibleVersionSql.ColumnInt(0);
  }

  std::optional<int> GetPrepopulatedFromMetaTable(sql::Database* db) {
    sql::Statement kGetPrepopulatedSql(db->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='prepopulated'"));
    if (!kGetPrepopulatedSql.Step()) {
      return std::nullopt;
    }
    return kGetPrepopulatedSql.ColumnInt(0);
  }

  std::optional<int64_t> GetPrepopulatedFromConfigTable(sql::Database* db) {
    sql::Statement kGetPrepopulatedSql(db->GetUniqueStatement(
        "SELECT int_value FROM config WHERE key='prepopulated'"));
    if (!kGetPrepopulatedSql.Step()) {
      return std::nullopt;
    }
    return kGetPrepopulatedSql.ColumnInt64(0);
  }

  std::string DbBouncesToString(sql::Database* db) {
    return sql::test::ExecuteWithResults(
        db, "SELECT * FROM bounces ORDER BY site", "|", "\n");
  }

  std::string DbPopupsToString(sql::Database* db) {
    return sql::test::ExecuteWithResults(
        db, "SELECT * FROM popups ORDER BY opener_site", "|", "\n");
  }

  std::string DbConfigToString(sql::Database* db) {
    return sql::test::ExecuteWithResults(
        db, "SELECT * FROM config ORDER BY key", "|", "\n");
  }

  std::vector<std::string> GetFirstAndLastColumnForSite(sql::Database* db,
                                                        const char* column,
                                                        const char* site) {
    std::string both_times = sql::test::ExecuteWithResults(
        db,
        base::StringPrintf("SELECT first_%s_time,last_%s_time FROM bounces "
                           "WHERE site='%s'",
                           column, column, site),
        "|", ",");

    return base::SplitString(both_times, "|", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_ALL);
  }

  void ExpectAllEntriesInColumnToBeNull(sql::Database* db,
                                        const char* table,
                                        const char* column) {
    const std::string num_rows_with_non_null_column_value =
        sql::test::ExecuteWithResult(
            db,
            base::StringPrintf("SELECT COUNT(*) FROM %s WHERE %s IS NOT NULL",
                               table, column));
    EXPECT_EQ(num_rows_with_non_null_column_value, "0");
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  // Test setup.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("DIPS.db");
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }
};

TEST_F(DIPSDatabaseMigrationTest, MigrateV1ToLatestVersion) {
  ASSERT_TRUE(LoadDatabase("v1.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 1);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 1);

    EXPECT_TRUE(db.DoesTableExist("bounces"));
    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_FALSE(db.DoesTableExist("config"));

    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|0|0|4|4|1|4|2|6\n"
              "stateful-bounce.test|0|0|4|4|1|1|0|0\n"
              "stateless-bounce.test|0|0|4|4|0|0|1|1\n"
              "storage.test|1|1|4|4|0|0|0|0");

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    MigrateDIPSSchemaToLatestVersion(db, meta_table);
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), DIPSDatabase::kLatestSchemaVersion);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db),
              DIPSDatabase::kMinCompatibleSchemaVersion);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "site"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_stateful_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_stateful_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_site_storage_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_site_storage_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_user_interaction_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_user_interaction_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_stateful_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_stateful_bounce_time"));
    EXPECT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));

    ASSERT_TRUE(db.DoesTableExist("popups"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "opener_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "popup_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "access_id"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "last_popup_time"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "is_current_interaction"));

    ASSERT_TRUE(db.DoesTableExist("config"));
    EXPECT_TRUE(db.DoesColumnExist("config", "key"));
    EXPECT_TRUE(db.DoesColumnExist("config", "int_value"));

    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|1|6||\n"
              "stateful-bounce.test|||4|4|1|1|1|1||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV1ToV2) {
  ASSERT_TRUE(LoadDatabase("v1.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 1);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_FALSE(db.DoesTableExist("config"));

    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_bounce_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    // These values are all set in v1.sql.
    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|0|0|4|4|1|4|2|6\n"
              "stateful-bounce.test|0|0|4|4|1|1|0|0\n"
              "stateless-bounce.test|0|0|4|4|0|0|1|1\n"
              "storage.test|1|1|4|4|0|0|0|0");

    // Note: that the stateful bounce happens earlier than the stateless bounce
    // this should be reflected in the first/last bounce times for this in v2.
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateful_bounce",
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateless_bounce",
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("2", "6"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateful_bounce",
                                             "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateless_bounce",
                                             "stateless-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "stateless-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "site_storage", "storage.test"),
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "user_interaction", "storage.test"),
        testing::ElementsAre("4", "4"));

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    migrator.MigrateSchemaVersionFrom1To2();
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 2);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 2);

    // Verify that all 0s were transformed to NULLs.
    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|1|6\n"
              "stateful-bounce.test|||4|4|1|1|1|1\n"
              "stateless-bounce.test|||4|4|||1|1\n"
              "storage.test|1|1|4|4||||");

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateful_bounce",
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "bounce", "both-bounce-kinds.test"),
        testing::ElementsAre(
            // first_bounce_time should be the minimum of
            // first_stateful_bounce_time and first_stateless_bounce_time.
            "1",
            // last_bounce_time should be the maximum of
            // last_stateful_bounce_time and last_stateless_bounce_time.
            "6"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "stateful_bounce",
                                             "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "bounce", "stateful-bounce.test"),
        // first_ and last_bounce_time should populate even if the old
        // *_stateless_bounce_time columns were null.
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "bounce", "stateless-bounce.test"),
        // first_ and last_bounce_time should populate even if the
        // *_stateful_bounce_times columns were null.
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, "user_interaction",
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "site_storage", "storage.test"),
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, "user_interaction", "storage.test"),
        testing::ElementsAre("4", "4"));
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV2ToV3) {
  ASSERT_TRUE(LoadDatabase("v2.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 2);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 2);
    EXPECT_EQ(GetPrepopulatedFromMetaTable(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_FALSE(db.DoesTableExist("config"));

    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6\n"
              "stateful-bounce.test|||4|4|1|1||\n"
              "stateless-bounce.test|||4|4|||1|1\n"
              "storage.test|1|1|4|4||||");

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 2, 2));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    migrator.MigrateSchemaVersionFrom2To3();
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 3);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 3);

    ASSERT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    ASSERT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    ExpectAllEntriesInColumnToBeNull(&db, "bounces",
                                     "first_web_authn_assertion_time");
    ExpectAllEntriesInColumnToBeNull(&db, "bounces",
                                     "last_web_authn_assertion_time");
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV3ToV4) {
  ASSERT_TRUE(LoadDatabase("v3.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 3);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 3);
    EXPECT_EQ(GetPrepopulatedFromMetaTable(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_FALSE(db.DoesTableExist("config"));

    EXPECT_EQ(DbBouncesToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6||\n"
              "stateful-bounce.test|||4|4|1|1||||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 3, 3));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    migrator.MigrateSchemaVersionFrom3To4();
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 4);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 4);

    EXPECT_TRUE(db.DoesTableExist("popups"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "opener_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "popup_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "access_id"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "last_popup_time"));
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV4ToV5) {
  ASSERT_TRUE(LoadDatabase("v4.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 4);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 4);
    EXPECT_EQ(GetPrepopulatedFromMetaTable(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("config"));

    EXPECT_TRUE(db.DoesColumnExist("popups", "opener_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "popup_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "access_id"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "last_popup_time"));

    EXPECT_EQ(DbPopupsToString(&db),
              "site1.com|3p-site.com|123|2023-10-01 12:00:00\n"
              "site2.com|3p-site.com|456|2023-10-02 12:00:00");

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 4, 4));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    migrator.MigrateSchemaVersionFrom4To5();
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 5);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 5);

    ASSERT_TRUE(db.DoesColumnExist("popups", "is_current_interaction"));
    ExpectAllEntriesInColumnToBeNull(&db, "popups", "is_current_interaction");
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV5ToV6) {
  ASSERT_TRUE(LoadDatabase("v5.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    ASSERT_EQ(GetDatabaseVersion(&db), 5);
    ASSERT_EQ(GetDatabaseLastCompatibleVersion(&db), 5);
    ASSERT_EQ(GetPrepopulatedFromMetaTable(&db), 1);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    ASSERT_TRUE(db.DoesTableExist("popups"));
    ASSERT_FALSE(db.DoesTableExist("config"));

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 5, 5));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    migrator.MigrateSchemaVersionFrom5To6();
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 6);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 6);

    EXPECT_TRUE(db.DoesTableExist("config"));
    EXPECT_TRUE(db.DoesColumnExist("config", "key"));
    EXPECT_TRUE(db.DoesColumnExist("config", "int_value"));

    EXPECT_EQ(GetPrepopulatedFromMetaTable(&db), std::nullopt);
    EXPECT_EQ(GetPrepopulatedFromConfigTable(&db), 1);
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV6ToV7) {
  ASSERT_TRUE(LoadDatabase("v6.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    ASSERT_EQ(GetDatabaseVersion(&db), 6);
    ASSERT_EQ(GetDatabaseLastCompatibleVersion(&db), 6);

    ASSERT_TRUE(db.DoesTableExist("config"));
    ASSERT_TRUE(db.DoesColumnExist("config", "key"));
    ASSERT_TRUE(db.DoesColumnExist("config", "int_value"));

    ASSERT_EQ(GetPrepopulatedFromConfigTable(&db), 1);

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 6, 6));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    ASSERT_TRUE(migrator.MigrateSchemaVersionFrom6To7());
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 7);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 6);

    EXPECT_TRUE(db.DoesTableExist("config"));
    EXPECT_TRUE(db.DoesColumnExist("config", "key"));
    EXPECT_TRUE(db.DoesColumnExist("config", "int_value"));

    EXPECT_EQ(GetPrepopulatedFromConfigTable(&db), std::nullopt);
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV7ToV8) {
  ASSERT_TRUE(LoadDatabase("v7.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Verify pre-migration conditions.

    ASSERT_EQ(GetDatabaseVersion(&db), 7);
    ASSERT_EQ(GetDatabaseLastCompatibleVersion(&db), 6);

    ASSERT_TRUE(db.DoesTableExist("config"));
    ASSERT_TRUE(db.DoesColumnExist("config", "key"));
    ASSERT_TRUE(db.DoesColumnExist("config", "int_value"));

    ASSERT_EQ(GetPrepopulatedFromConfigTable(&db), std::nullopt);

    // Migrate.

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 7, 6));

    sql::Transaction transaction(&db);
    ASSERT_TRUE(transaction.Begin());
    DIPSDatabaseMigrator migrator(&db, &meta_table);
    ASSERT_TRUE(migrator.MigrateSchemaVersionFrom7To8());
    ASSERT_TRUE(transaction.Commit());

    // Verify post-migration conditions.

    EXPECT_EQ(GetDatabaseVersion(&db), 8);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 8);

    ASSERT_TRUE(db.DoesColumnExist("popups", "is_authentication_interaction"));
    ExpectAllEntriesInColumnToBeNull(&db, "popups",
                                     "is_authentication_interaction");
  }
}
