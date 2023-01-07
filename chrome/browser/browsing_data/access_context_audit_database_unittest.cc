// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_database.h"

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace {

// Define an arbitrary ordering to allow sorting of AccessRecords for easier
// testing, as no ordering is guaranteed by the database.
bool RecordTestOrdering(const AccessContextAuditDatabase::AccessRecord& a,
                        const AccessContextAuditDatabase::AccessRecord& b) {
  if (a.last_access_time != b.last_access_time)
    return a.last_access_time < b.last_access_time;

  if (a.top_frame_origin != b.top_frame_origin)
    return a.top_frame_origin < b.top_frame_origin;

  return a.type < b.type;
}

void ExpectAccessRecordsEqual(
    const AccessContextAuditDatabase::AccessRecord& a,
    const AccessContextAuditDatabase::AccessRecord& b) {
  if (a.top_frame_origin.opaque()) {
    EXPECT_TRUE(b.top_frame_origin.opaque());
  } else {
    EXPECT_EQ(a.top_frame_origin, b.top_frame_origin);
  }
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.last_access_time, b.last_access_time);

  if (a.type == AccessContextAuditDatabase::StorageAPIType::kCookie) {
    EXPECT_EQ(a.name, b.name);
    EXPECT_EQ(a.domain, b.domain);
    EXPECT_EQ(a.path, b.path);
    EXPECT_EQ(a.is_persistent, b.is_persistent);
  } else {
    EXPECT_EQ(a.origin, b.origin);
  }
}

void ValidateRecords(
    std::vector<AccessContextAuditDatabase::AccessRecord> got_records,
    std::vector<AccessContextAuditDatabase::AccessRecord> want_records) {
  // Apply an arbitrary ordering to simplify testing equivalence.
  std::sort(got_records.begin(), got_records.end(), RecordTestOrdering);
  std::sort(want_records.begin(), want_records.end(), RecordTestOrdering);

  EXPECT_EQ(got_records.size(), want_records.size());
  for (size_t i = 0; i < std::min(got_records.size(), want_records.size());
       i++) {
    ExpectAccessRecordsEqual(got_records[i], want_records[i]);
  }
}

void ValidateDatabaseRecords(
    AccessContextAuditDatabase* database,
    std::vector<AccessContextAuditDatabase::AccessRecord> expected_records) {
  ValidateRecords(database->GetAllRecords(), expected_records);
}

constexpr char kManyContextsCookieName[] = "multiple contexts cookie";
constexpr char kManyContextsCookieDomain[] = "multi-contexts.com";
constexpr char kManyContextsCookiePath[] = "/";
constexpr char kManyContextsStorageAPIOrigin[] = "https://many-contexts.com";
constexpr char kManyVisitsTopFrameOrigin1[] = "https://mulitple-visits.com";
constexpr char kManyVisitsTopFrameOrigin2[] = "https://mulitple-other.com";
constexpr AccessContextAuditDatabase::StorageAPIType
    kManyContextsStorageAPIType =
        AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
constexpr AccessContextAuditDatabase::StorageAPIType
    kSingleContextStorageAPIType =
        AccessContextAuditDatabase::StorageAPIType::kIndexedDB;

}  // namespace

class AccessContextAuditDatabaseTest : public testing::Test {
 public:
  AccessContextAuditDatabaseTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase(bool restore_non_persistent_cookies = false) {
    database_.reset();
    database_ = base::MakeRefCounted<AccessContextAuditDatabase>(
        temp_directory_.GetPath());
    database_->Init(restore_non_persistent_cookies);
  }

  void CloseDatabase() { database_.reset(); }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("AccessContextAudit"));
  }

  AccessContextAuditDatabase* database() { return database_.get(); }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetTestRecords() {
    return {
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin1)),
            AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
            url::Origin::Create(GURL("https://test.com")),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(1))),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin2)),
            AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
            url::Origin::Create(GURL("https://test.com")),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(2))),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL("https://test2.com:8000")), "cookie1",
            "test.com", "/",
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(3)),
            /* is_persistent */ true),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin1)),
            kManyContextsCookieName, kManyContextsCookieDomain,
            kManyContextsCookiePath,
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(4)),
            /* is_persistent */ true),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin2)),
            kManyContextsCookieName, kManyContextsCookieDomain,
            kManyContextsCookiePath,
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(4)),
            /* is_persistent */ true),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL("https://test4.com:8000")),
            kManyContextsStorageAPIType,
            url::Origin::Create(GURL(kManyContextsStorageAPIOrigin)),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(5))),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin1)),
            kManyContextsStorageAPIType,
            url::Origin::Create(GURL(kManyContextsStorageAPIOrigin)),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(6))),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL(kManyVisitsTopFrameOrigin2)),
            kSingleContextStorageAPIType,
            url::Origin::Create(GURL(kManyContextsStorageAPIOrigin)),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(7))),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL("https://test6.com")),
            "non-persistent-cookie", "non-persistent-domain", "/",
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(8)),
            /* is_persistent */ false),
        AccessContextAuditDatabase::AccessRecord(
            url::Origin::Create(GURL("https://test7.com")),
            kManyContextsStorageAPIType,
            url::Origin::Create(GURL("https://test8.com")),
            base::Time::FromDeltaSinceWindowsEpoch(base::Hours(9))),
    };
  }

 private:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<AccessContextAuditDatabase> database_;
};

TEST_F(AccessContextAuditDatabaseTest, DatabaseInitialization) {
  // Check that tables are created and at least have the appropriate number of
  // columns.
  OpenDatabase();
  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  // Database is currently at version 1.
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(&raw_db, 1, 1));

  // [cookies], [storageapi] and [meta]
  EXPECT_EQ(3u, sql::test::CountSQLTables(&raw_db));

  // [top_frame_origin, name, domain, path, access_utc, is_persistent]
  EXPECT_EQ(6u, sql::test::CountTableColumns(&raw_db, "cookies"));

  // [top_frame_origin, type, origin, access_utc]
  EXPECT_EQ(4u, sql::test::CountTableColumns(&raw_db, "originStorageAPIs"));
}

TEST_F(AccessContextAuditDatabaseTest, ComputeDatabaseMetrics) {
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  base::HistogramTester histogram_tester;
  database()->ComputeDatabaseMetrics();

  // Check total number of records was recorded.
  histogram_tester.ExpectUniqueSample("Privacy.AccessContextAudit.RecordCount",
                                      test_records.size(), 1);

  // Check database file size was recorded in KB.
  int64_t file_size;
  base::GetFileSize(db_path(), &file_size);
  histogram_tester.ExpectUniqueSample("Privacy.AccessContextAudit.DatabaseSize",
                                      static_cast<int>(file_size / 1024), 1);

  // Check unique top frame origin count was recorded.
  std::set<url::Origin> top_frame_origins;
  for (const auto& record : test_records)
    top_frame_origins.insert(record.top_frame_origin);
  histogram_tester.ExpectUniqueSample(
      "Privacy.AccessContextAudit.TopFrameOriginCount",
      top_frame_origins.size(), 1);

  // Check unique cookie domain count was recorded.
  std::set<std::string> cookie_domains;
  for (const auto& record : test_records) {
    if (record.type == AccessContextAuditDatabase::StorageAPIType::kCookie)
      cookie_domains.insert(record.domain);
  }
  histogram_tester.ExpectUniqueSample(
      "Privacy.AccessContextAudit.CookieDomainCount", cookie_domains.size(), 1);

  // Check unique cookie domain count was recorded.
  std::set<url::Origin> storage_origins;
  for (const auto& record : test_records) {
    if (record.type != AccessContextAuditDatabase::StorageAPIType::kCookie)
      storage_origins.insert(record.origin);
  }
  histogram_tester.ExpectUniqueSample(
      "Privacy.AccessContextAudit.StorageOriginCount", storage_origins.size(),
      1);
}

TEST_F(AccessContextAuditDatabaseTest, IsPersistentSchemaMigration) {
  // Check that the database opens and functions correctly when a database
  // with a cookies table, but without an is_persistent field, is present.
  auto test_records = GetTestRecords();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  // Create a cookies table without is_persistent.
  EXPECT_TRUE(
      raw_db.Execute("CREATE TABLE cookies "
                     "(top_frame_origin TEXT NOT NULL,"
                     "name TEXT NOT NULL,"
                     "domain TEXT NOT NULL,"
                     "path TEXT NOT NULL,"
                     "access_utc INTEGER NOT NULL,"
                     "PRIMARY KEY (top_frame_origin, name, domain, path))"));

  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RestoreNonPersistentCookies) {
  // Check that non-persistent records are preserved with all other records
  // when the database is opened with the restore_non_persistent_cookies flag.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  CloseDatabase();
  OpenDatabase(/* restore_non_persistent_cookies */ true);

  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();
}

TEST_F(AccessContextAuditDatabaseTest, NonPersistentCookiesRemoved) {
  // Check that non-persistent records are removed, but all other records are
  // retained when the database is opened without the
  // restore_non_persistent_cookies flag.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  CloseDatabase();
  OpenDatabase();

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   record.is_persistent == false;
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();
}

TEST_F(AccessContextAuditDatabaseTest, RazedOnError) {
  // Check that the database is razed when opening a corrupted file.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(
      OpenDatabase(/* restore_non_persistent_cookies */ true));

  // No data should be present as the database should have been razed.
  ValidateDatabaseRecords(database(), {});

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

TEST_F(AccessContextAuditDatabaseTest, RemoveRecord) {
  // Check that entries are removed from the database such that they are both
  // not returned by GetAllRecords and are removed from the database file.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  while (test_records.size() > 0) {
    database()->RemoveRecord(test_records[0]);
    test_records.erase(test_records.begin());
    ValidateDatabaseRecords(database(), test_records);
  }
  CloseDatabase();

  // Verify that everything is deleted.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(0u, cookie_rows);
  EXPECT_EQ(0u, storage_api_rows);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllRecords) {
  // Check that removing all records deleted all entries from the database and
  // from the database file.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  database()->RemoveAllRecords();
  ValidateDatabaseRecords(database(), {});
  CloseDatabase();

  // Verify that everything is deleted from the database file.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(0u, cookie_rows);
  EXPECT_EQ(0u, storage_api_rows);
}

// Check that this method removes all records when third-party data clearing is
// not enabled.
TEST_F(AccessContextAuditDatabaseTest, RemoveAllRecordsHistory) {
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  database()->RemoveAllRecordsHistory();
  ValidateDatabaseRecords(database(), {});
  CloseDatabase();

  // Verify that everything is deleted from the database file.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(0u, cookie_rows);
  EXPECT_EQ(0u, storage_api_rows);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllRecordsForTimeRange) {
  // Check that records within the specified time range are removed.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  auto begin_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(4));
  auto end_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(6));

  database()->RemoveAllRecordsForTimeRange(begin_time, end_time);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.last_access_time >= begin_time &&
                   record.last_access_time <= end_time;
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllRecordsForTimeRangeHistory) {
  // Check that records within the specified time range are removed.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  auto begin_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(4));
  auto end_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(6));

  database()->RemoveAllRecordsForTimeRangeHistory(begin_time, end_time);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.last_access_time >= begin_time &&
                   record.last_access_time <= end_time;
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllCookieRecords) {
  // Check that all matching cookie records are removed from the database.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  database()->RemoveAllRecordsForCookie(kManyContextsCookieName,
                                        kManyContextsCookieDomain,
                                        kManyContextsCookiePath);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return (record.type ==
                        AccessContextAuditDatabase::StorageAPIType::kCookie &&
                    record.name == kManyContextsCookieName &&
                    record.domain == kManyContextsCookieDomain &&
                    record.path == kManyContextsCookiePath);
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllRecordsForTopFrameOrigins) {
  // Check that all records which have one of the provided origins as the top
  // frame origin are removed correctly.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  std::vector<url::Origin> many_visit_origins = {
      url::Origin::Create(GURL(kManyVisitsTopFrameOrigin1)),
      url::Origin::Create(GURL(kManyVisitsTopFrameOrigin2))};
  database()->RemoveAllRecordsForTopFrameOrigins(many_visit_origins);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return base::Contains(many_visit_origins, record.top_frame_origin);
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveAllStorageRecords) {
  // Check that all records matching the provided origin and storage type
  // are removed.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  database()->RemoveAllRecordsForOriginKeyedStorage(
      url::Origin::Create(GURL(kManyContextsStorageAPIOrigin)),
      kManyContextsStorageAPIType);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return (record.type == kManyContextsStorageAPIType &&
                    record.origin == url::Origin::Create(
                                         GURL(kManyContextsStorageAPIOrigin)));
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RepeatedAccesses) {
  // Check that additional access records, only differing by timestamp to
  // previous entries, update those entries rather than creating new ones.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  for (auto& record : test_records) {
    record.last_access_time += base::Hours(1);
  }

  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();

  // Verify that extra entries are not present in the database.
  size_t num_test_cookie_entries = base::ranges::count(
      test_records, AccessContextAuditDatabase::StorageAPIType::kCookie,
      &AccessContextAuditDatabase::AccessRecord::type);
  size_t num_test_storage_entries =
      test_records.size() - num_test_cookie_entries;

  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(num_test_cookie_entries, cookie_rows);
  EXPECT_EQ(num_test_storage_entries, storage_api_rows);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveSessionOnlyRecords) {
  // Check that records are cleared appropriately by RemoveSessionOnlyRecords
  // when the provided content settings indicate they should.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  // Test that default content setting of SESSION_ONLY results in all records
  // being removed.
  ContentSettingsForOneType content_settings;
  content_settings.emplace_back(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_SESSION_ONLY),
      std::string(), /* incognito */ false);

  database()->RemoveSessionOnlyRecords(content_settings);
  ValidateDatabaseRecords(database(), {});

  // Check that a more targeted content setting also removes the appropriate
  // records.
  content_settings.clear();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  content_settings.emplace_back(
      ContentSettingsPattern::FromString(kManyContextsCookieDomain),
      ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_SESSION_ONLY),
      std::string(), /* incognito */ false);
  content_settings.emplace_back(
      ContentSettingsPattern::FromString(kManyContextsStorageAPIOrigin),
      ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_SESSION_ONLY),
      std::string(), /* incognito */ false);
  content_settings.emplace_back(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), /* incognito */ false);
  database()->RemoveSessionOnlyRecords(content_settings);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            if (record.type ==
                AccessContextAuditDatabase::StorageAPIType::kCookie) {
              return record.domain == kManyContextsCookieDomain;
            }
            return record.origin ==
                   url::Origin::Create(GURL(kManyContextsStorageAPIOrigin));
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, RemoveStorageApiRecords) {
  // Check that removal of storage API records based on a combined time range,
  // storage type, and origin matching function operates correctly.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  std::set<AccessContextAuditDatabase::StorageAPIType> storage_types;
  storage_types.insert(kManyContextsStorageAPIType);
  storage_types.insert(kSingleContextStorageAPIType);

  auto kStorageOrigin =
      url::Origin::Create(GURL(kManyContextsStorageAPIOrigin));

  auto storage_key_matcher =
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey(kStorageOrigin);
      });

  auto begin_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(5));
  auto end_time = base::Time::FromDeltaSinceWindowsEpoch(base::Hours(9));

  database()->RemoveStorageApiRecords(storage_types, storage_key_matcher,
                                      begin_time, end_time);
  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return (record.type == kManyContextsStorageAPIType ||
                    record.type == kSingleContextStorageAPIType) &&
                   record.origin == kStorageOrigin &&
                   record.last_access_time >= begin_time &&
                   record.last_access_time <= end_time;
          }),
      test_records.end());

  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);

  // Check that providing a null origin matcher function matches all origins.
  test_records = GetTestRecords();
  database()->AddRecords(test_records);

  database()->RemoveStorageApiRecords(storage_types, base::NullCallback(),
                                      begin_time, end_time);

  test_records.erase(
      std::remove_if(
          test_records.begin(), test_records.end(),
          [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return (record.type == kManyContextsStorageAPIType ||
                    record.type == kSingleContextStorageAPIType) &&
                   record.last_access_time >= begin_time &&
                   record.last_access_time <= end_time;
          }),
      test_records.end());
  EXPECT_GT(GetTestRecords().size(), test_records.size());
  ValidateDatabaseRecords(database(), test_records);
}

TEST_F(AccessContextAuditDatabaseTest, GetCookieRecords) {
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  std::vector<AccessContextAuditDatabase::AccessRecord> want_records;
  for (auto& record : test_records) {
    if (record.type == AccessContextAuditDatabase::StorageAPIType::kCookie) {
      want_records.push_back(record);
    }
  }

  ValidateRecords(database()->GetCookieRecords(), want_records);
}

TEST_F(AccessContextAuditDatabaseTest, GetStorageRecords) {
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  std::vector<AccessContextAuditDatabase::AccessRecord> want_records;
  for (auto& record : test_records) {
    if (record.type != AccessContextAuditDatabase::StorageAPIType::kCookie) {
      want_records.push_back(record);
    }
  }

  ValidateRecords(database()->GetStorageRecords(), want_records);
}

class AccessContextAuditDatabaseThirdPartyDataClearingTest
    : public AccessContextAuditDatabaseTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {browsing_data::features::kEnableRemovingAllThirdPartyCookies}, {});
    AccessContextAuditDatabaseTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AccessContextAuditDatabaseThirdPartyDataClearingTest,
       RemoveAllRecordsForTopFrameOrigins) {
  const url::Origin kTopLevelOrigin1 =
      url::Origin::Create(GURL("https://toplevel1.com/"));
  const url::Origin kTopLevelOrigin2 =
      url::Origin::Create(GURL("https://toplevel2.com/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("https://cross.site.com/"));
  const base::Time kAccessTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(1));
  const base::Time kLaterAccessTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(2));

  std::vector<AccessContextAuditDatabase::AccessRecord> test_records = {
      // Same-site and cross-site cookie records must be removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin1, "samesite", "toplevel1.com", "/", kAccessTime,
          /* is_persistent */ true),
      AccessContextAuditDatabase::AccessRecord(kTopLevelOrigin1, "xsite",
                                               "xsite.com", "/", kAccessTime,
                                               /* is_persistent */ true),
      // Same-site storage access record. This should be removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin1,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kTopLevelOrigin1, kAccessTime),
      // Cross-site storage access records. These should both coalesce to the
      // same storage record, since the top-level origin will be removed and
      // replaced with an opaque origin.
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin1,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kCrossSiteOrigin, kAccessTime),
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin2,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kCrossSiteOrigin, kLaterAccessTime),
  };
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  database()->RemoveAllRecordsForTopFrameOrigins(
      {kTopLevelOrigin1, kTopLevelOrigin2});

  ValidateDatabaseRecords(
      database(),
      {
          AccessContextAuditDatabase::AccessRecord(
              url::Origin(),
              AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
              kCrossSiteOrigin, kLaterAccessTime),
      });
}

TEST_F(AccessContextAuditDatabaseThirdPartyDataClearingTest,
       RemoveAllRecordsHistory) {
  const url::Origin kTopLevelOrigin1 =
      url::Origin::Create(GURL("https://toplevel1.com/"));
  const url::Origin kTopLevelOrigin2 =
      url::Origin::Create(GURL("https://toplevel2.com/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("https://cross.site.com/"));
  const base::Time kAccessTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(1));
  const base::Time kLaterAccessTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(2));

  std::vector<AccessContextAuditDatabase::AccessRecord> test_records = {
      // Same-site and cross-site cookie records must be removed.
      AccessContextAuditDatabase::AccessRecord(kTopLevelOrigin1, "samesite",
                                               "toplevel.com", "/", kAccessTime,
                                               /* is_persistent */ true),
      AccessContextAuditDatabase::AccessRecord(kTopLevelOrigin1, "xsite",
                                               "xsite.com", "/", kAccessTime,
                                               /* is_persistent */ true),
      // Same-site storage access record. This should be removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin1,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kTopLevelOrigin1, kAccessTime),
      // Cross-site storage access records. These should both coalesce to the
      // same storage record, since the top-level origin will be removed and
      // replaced with an opaque origin.
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin1,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kCrossSiteOrigin, kAccessTime),
      AccessContextAuditDatabase::AccessRecord(
          kTopLevelOrigin2,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kCrossSiteOrigin, kLaterAccessTime),
  };
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  database()->RemoveAllRecordsHistory();
  ValidateDatabaseRecords(
      database(),
      {
          AccessContextAuditDatabase::AccessRecord(
              url::Origin(),
              AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
              kCrossSiteOrigin, kLaterAccessTime),
      });
}

TEST_F(AccessContextAuditDatabaseThirdPartyDataClearingTest,
       RemoveAllRecordsForTimeRangeHistory) {
  const url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://toplevel.com/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("https://cross.site.com/"));

  const base::Time kBeginTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(4));
  const base::Time kEndTime =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(6));
  const base::Time kInsideRange =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(5));
  const base::Time kOutsideRange =
      base::Time::FromDeltaSinceWindowsEpoch(base::Hours(7));

  std::vector<AccessContextAuditDatabase::AccessRecord> test_records = {
      // Same-site and cross-site cookie records in the time range must be
      // removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopFrameOrigin, "samesite", "toplevel.com", "/", kInsideRange,
          /* is_persistent */ true),
      AccessContextAuditDatabase::AccessRecord(kTopFrameOrigin, "xsite",
                                               "xsite.com", "/", kInsideRange,
                                               /* is_persistent */ true),
      // Cookie records outside the time range should remain.
      AccessContextAuditDatabase::AccessRecord(
          kTopFrameOrigin, "outside", "toplevel.com", "/", kOutsideRange,
          /* is_persistent */ true),
      // Same-site storage access record inside the itme range. This should be
      // removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopFrameOrigin,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kTopFrameOrigin, kInsideRange),
      // Cross-site record inside the time range should have its top-level
      // origin removed.
      AccessContextAuditDatabase::AccessRecord(
          kTopFrameOrigin,
          AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
          kCrossSiteOrigin, kInsideRange),
      // Cross-site record outside the time range should not be modified.
      AccessContextAuditDatabase::AccessRecord(
          kTopFrameOrigin,
          AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
          kCrossSiteOrigin, kOutsideRange),
  };
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  database()->RemoveAllRecordsForTimeRangeHistory(kBeginTime, kEndTime);
  ValidateDatabaseRecords(
      database(),
      {
          AccessContextAuditDatabase::AccessRecord(
              kTopFrameOrigin, "outside", "toplevel.com", "/", kOutsideRange,
              /* is_persistent */ true),
          AccessContextAuditDatabase::AccessRecord(
              url::Origin(),
              AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
              kCrossSiteOrigin, kInsideRange),
          AccessContextAuditDatabase::AccessRecord(
              kTopFrameOrigin,
              AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
              kCrossSiteOrigin, kOutsideRange),
      });
}
