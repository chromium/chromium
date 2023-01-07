// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace {

// Checks that a record exists in |records| that matches both |cookie| and
// |top_frame_origin|.
void CheckContainsCookieRecord(
    net::CanonicalCookie* cookie,
    url::Origin top_frame_origin,
    base::Time last_access_time,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& records) {
  EXPECT_TRUE(
      base::ranges::any_of(
          records, [=](const AccessContextAuditDatabase::AccessRecord& record) {
            return record.type ==
                       AccessContextAuditDatabase::StorageAPIType::kCookie &&
                   record.top_frame_origin == top_frame_origin &&
                   record.name == cookie->Name() &&
                   record.domain == cookie->Domain() &&
                   record.path == cookie->Path() &&
                   record.last_access_time == last_access_time &&
                   record.is_persistent == cookie->IsPersistent();
          }));
}

// Checks that info in |record| matches storage API access defined by
// |storage_origin|, |type| and |top_frame_origin|
void CheckContainsStorageAPIRecord(
    url::Origin storage_origin,
    AccessContextAuditDatabase::StorageAPIType type,
    url::Origin top_frame_origin,
    base::Time last_access_time,
    const std::vector<AccessContextAuditDatabase::AccessRecord>& records) {
  EXPECT_TRUE(base::ranges::any_of(
      records, [=](const AccessContextAuditDatabase::AccessRecord& record) {
        return record.type == type && record.origin == storage_origin &&
               record.top_frame_origin == top_frame_origin &&
               record.last_access_time == last_access_time;
      }));
}

}  // namespace

class TestCookieManager : public network::TestCookieManager {
 public:
  void AddGlobalChangeListener(
      mojo::PendingRemote<network::mojom::CookieChangeListener>
          notification_pointer) override {
    listener_registered_ = true;
  }

  bool ListenerRegistered() { return listener_registered_; }

 protected:
  bool listener_registered_ = false;
};

class AccessContextAuditServiceTest : public testing::Test {
 public:
  AccessContextAuditServiceTest() = default;

  std::unique_ptr<KeyedService> BuildTestContextAuditService(
      content::BrowserContext* context) {
    auto service = std::make_unique<AccessContextAuditService>(
        static_cast<Profile*>(context));
    service->SetTaskRunnerForTesting(task_runner_);
    service->Init(temp_directory_.GetPath(), cookie_manager(),
                  history_service(), storage_partition());
    return service;
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(enabled_features(), disabled_features());

    task_runner_ = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::ThreadPolicy::PREFER_BACKGROUND,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_directory_.GetPath()));

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        AccessContextAuditServiceFactory::GetInstance(),
        base::BindRepeating(
            &AccessContextAuditServiceTest::BuildTestContextAuditService,
            base::Unretained(this)));
    builder.SetPath(temp_directory_.GetPath());
    profile_ = builder.Build();
    FlushSequencedTaskRunner();
    browser_task_environment_.RunUntilIdle();
  }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetAllAccessRecords() {
    base::RunLoop run_loop;
    std::vector<AccessContextAuditDatabase::AccessRecord> records_out;
    service()->GetAllAccessRecords(base::BindLambdaForTesting(
        [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
          records_out = records;
          run_loop.QuitWhenIdle();
        }));
    run_loop.Run();
    return records_out;
  }

  void FlushSequencedTaskRunner() {
    base::RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting(
                                          [&]() { run_loop.QuitWhenIdle(); }));
    run_loop.Run();
  }

  TestCookieManager* cookie_manager() { return &cookie_manager_; }
  content::TestStoragePartition* storage_partition() {
    return &storage_partition_;
  }
  base::SimpleTestClock* clock() { return &clock_; }
  TestingProfile* profile() { return profile_.get(); }
  history::HistoryService* history_service() { return history_service_.get(); }
  AccessContextAuditService* service() {
    return AccessContextAuditServiceFactory::GetForProfile(profile());
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<TestingProfile> profile_;
  base::SimpleTestClock clock_;
  base::ScopedTempDir temp_directory_;
  TestCookieManager cookie_manager_;
  content::TestStoragePartition storage_partition_;
  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner_;
  std::vector<AccessContextAuditDatabase::AccessRecord> records_;

  virtual std::vector<base::test::FeatureRef> enabled_features() {
    return {features::kClientStorageAccessContextAuditing};
  }
  virtual std::vector<base::test::FeatureRef> disabled_features() {
    return {browsing_data::features::kEnableRemovingAllThirdPartyCookies};
  }
};

TEST_F(AccessContextAuditServiceTest, RegisterDeletionObservers) {
  // Check that the service correctly registers observers for deletion.
  EXPECT_TRUE(cookie_manager_.ListenerRegistered());
  EXPECT_EQ(1, storage_partition()->GetDataRemovalObserverCount());
}

TEST_F(AccessContextAuditServiceTest, CookieRecords) {
  // Check that cookie access records are successfully stored and deleted.
  GURL kTestCookieURL("https://example.com");
  std::string kTestCookieName = "test";
  std::string kTestNonPersistentCookieName = "test-non-persistent";
  const base::Time kAccessTime1 = base::Time::Now();
  clock()->SetNow(kAccessTime1);
  service()->SetClockForTesting(clock());

  auto test_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestCookieName + "=1; max-age=3600", kAccessTime1,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto test_non_persistent_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestNonPersistentCookieName + "=1", kAccessTime1,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  // Record access to these cookies against a URL.
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  service()->RecordCookieAccess({*test_cookie, *test_non_persistent_cookie},
                                kTopFrameOrigin);

  // Ensure that the record of these accesses is correctly returned.
  auto records = GetAllAccessRecords();
  EXPECT_EQ(2u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin, kAccessTime1,
                            records);
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            kAccessTime1, records);

  // Check that informing the service of non-deletion changes to the cookies
  // via the CookieChangeInterface is a no-op.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessResult(),
                            net::CookieChangeCause::OVERWRITE));
  service()->OnCookieChange(net::CookieChangeInfo(
      *test_non_persistent_cookie, net::CookieAccessResult(),
      net::CookieChangeCause::OVERWRITE));

  records = GetAllAccessRecords();
  EXPECT_EQ(2u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin, kAccessTime1,
                            records);
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            kAccessTime1, records);

  // Check that a repeated access correctly updates associated timestamp.
  clock()->Advance(base::Hours(1));
  const base::Time kAccessTime2 = clock()->Now();
  service()->RecordCookieAccess({*test_cookie, *test_non_persistent_cookie},
                                kTopFrameOrigin);

  records = GetAllAccessRecords();
  EXPECT_EQ(2u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin, kAccessTime2,
                            records);
  CheckContainsCookieRecord(test_non_persistent_cookie.get(), kTopFrameOrigin,
                            kAccessTime2, records);

  // Test GetCookieRecords by inserting a non-cookie record and then make
  // sure GetCookieRecords does not include it in the result.
  service()->RecordStorageAPIAccess(
      url::Origin::Create(kTestCookieURL),
      AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
      kTopFrameOrigin);
  EXPECT_EQ(3u, GetAllAccessRecords().size());
  base::RunLoop run_loop;
  std::vector<AccessContextAuditDatabase::AccessRecord> cookie_records;
  service()->GetCookieAccessRecords(base::BindLambdaForTesting(
      [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
        cookie_records = records;
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  EXPECT_EQ(2u, cookie_records.size());
  for (auto cr : cookie_records) {
    EXPECT_EQ(AccessContextAuditDatabase::StorageAPIType::kCookie, cr.type);
  }
  service()->RemoveAllRecordsForOriginKeyedStorage(
      url::Origin::Create(kTestCookieURL),
      AccessContextAuditDatabase::StorageAPIType::kLocalStorage);
  EXPECT_EQ(2u, GetAllAccessRecords().size());

  // Inform the service the cookies have been deleted and check they are no
  // longer returned.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessResult(),
                            net::CookieChangeCause::EXPLICIT));
  service()->OnCookieChange(net::CookieChangeInfo(
      *test_non_persistent_cookie, net::CookieAccessResult(),
      net::CookieChangeCause::EXPLICIT));
  records = GetAllAccessRecords();
  EXPECT_EQ(0u, records.size());
}

TEST_F(AccessContextAuditServiceTest, ExpiredCookies) {
  // Check that no accesses are recorded for cookies which have already expired.
  const GURL kTestURL("https://test.com");
  auto test_cookie_expired = net::CanonicalCookie::Create(
      kTestURL, "test_1=1; expires=Thu, 01 Jan 1970 00:00:00 GMT",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  service()->RecordCookieAccess({*test_cookie_expired},
                                url::Origin::Create(kTestURL));

  EXPECT_EQ(0u, GetAllAccessRecords().size());
}

TEST_F(AccessContextAuditServiceTest, GetStorageRecords) {
  GURL kTestUrl = GURL("https://example.com");
  const base::Time kAccessTime1 = base::Time::Now();

  // Insert a cookie access and several storage access records into the
  // database.
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(kTestUrl, "foo=bar; max-age=3600",
                                     kAccessTime1,
                                     absl::nullopt /* server_time */,
                                     absl::nullopt /* cookie_partition_key */)},
      kTopFrameOrigin);
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);
  service()->RecordStorageAPIAccess(
      kTestOrigin, AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
      kTopFrameOrigin);
  service()->RecordStorageAPIAccess(
      kTestOrigin, AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
      kTopFrameOrigin);
  service()->RecordStorageAPIAccess(
      kTestOrigin, AccessContextAuditDatabase::StorageAPIType::kServiceWorker,
      kTopFrameOrigin);
  EXPECT_EQ(4u, GetAllAccessRecords().size());

  base::RunLoop run_loop;
  std::vector<AccessContextAuditDatabase::AccessRecord> storage_records;
  service()->GetStorageAccessRecords(base::BindLambdaForTesting(
      [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
        storage_records = records;
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  EXPECT_EQ(3u, storage_records.size());
  for (auto sr : storage_records) {
    EXPECT_NE(AccessContextAuditDatabase::StorageAPIType::kCookie, sr.type);
  }
}

TEST_F(AccessContextAuditServiceTest, GetThirdPartyStorageRecords) {
  GURL kTestUrl = GURL("https://example.com");
  const base::Time kAccessTime1 = base::Time::Now();
  url::Origin kTestOrigin = url::Origin::Create(kTestUrl);
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));

  // Add a record of storage being accessed in a third-party context.
  service()->RecordStorageAPIAccess(
      kTestOrigin, AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
      kTopFrameOrigin);
  // Add records of a cookie access and a storage access in a first-party
  // context. These should be included in GetAllAccessRecords() but excluded
  // from GetThirdPartyStorageAccessRecords().
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(kTestUrl, "foo=bar; max-age=3600",
                                     kAccessTime1,
                                     absl::nullopt /* server_time */,
                                     absl::nullopt /* cookie_partition_key */)},
      kTopFrameOrigin);
  service()->RecordStorageAPIAccess(
      kTestOrigin, AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
      kTestOrigin);
  EXPECT_EQ(3u, GetAllAccessRecords().size());

  base::RunLoop run_loop;
  std::vector<AccessContextAuditDatabase::AccessRecord> storage_records;
  service()->GetThirdPartyStorageAccessRecords(base::BindLambdaForTesting(
      [&](std::vector<AccessContextAuditDatabase::AccessRecord> records) {
        storage_records = records;
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  EXPECT_EQ(1u, storage_records.size());
  EXPECT_EQ(AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
            storage_records[0].type);
}

TEST_F(AccessContextAuditServiceTest, OriginKeyedStorageDeleted) {
  // Check that informing the service that an origin's storage of a particular
  // type as been deleted removes all records of that storage.
  const auto kTestStorageType1 =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
  const auto kTestStorageType2 =
      AccessContextAuditDatabase::StorageAPIType::kIndexedDB;
  const url::Origin kTestOrigin1 =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://example2.com"));
  const url::Origin kTestTopLevelOrigin =
      url::Origin::Create(GURL("https://example3.com"));
  const base::Time kAccessTime = base::Time::Now();
  clock()->SetNow(kAccessTime);
  service()->SetClockForTesting(clock());

  // Record accesses for the 4 possible test type and origin combinations.
  service()->RecordStorageAPIAccess(kTestOrigin1, kTestStorageType1,
                                    kTestTopLevelOrigin);
  service()->RecordStorageAPIAccess(kTestOrigin1, kTestStorageType2,
                                    kTestTopLevelOrigin);
  service()->RecordStorageAPIAccess(kTestOrigin2, kTestStorageType1,
                                    kTestTopLevelOrigin);
  service()->RecordStorageAPIAccess(kTestOrigin2, kTestStorageType2,
                                    kTestTopLevelOrigin);

  // Remove records for Origin1 and Type1 and ensure the record is removed, but
  // those for Origin2 or Type2 are not.
  service()->RemoveAllRecordsForOriginKeyedStorage(kTestOrigin1,
                                                   kTestStorageType1);

  auto records = GetAllAccessRecords();
  EXPECT_EQ(3u, records.size());
  CheckContainsStorageAPIRecord(kTestOrigin1, kTestStorageType2,
                                kTestTopLevelOrigin, kAccessTime, records);
  CheckContainsStorageAPIRecord(kTestOrigin2, kTestStorageType1,
                                kTestTopLevelOrigin, kAccessTime, records);
  CheckContainsStorageAPIRecord(kTestOrigin2, kTestStorageType2,
                                kTestTopLevelOrigin, kAccessTime, records);
}

TEST_F(AccessContextAuditServiceTest, HistoryDeletion) {
  // Check when the last record of an origin is deleted from history all records
  // with it as a top frame origin are also removed.
  const auto kTestStorageType =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
  const url::Origin kTestStorageOrigin =
      url::Origin::Create(GURL("http://test.com"));
  const GURL kTestCookieURL("https://example.com");
  const std::string kTestCookieName = "test";
  const GURL kURL1 = GURL("https://remaining-entries.com/test1");
  const GURL kURL2 = GURL("https://remaining-entries.com/test2");
  const GURL kURL3 = GURL("https://no-remaining-entries.com/test1");
  const url::Origin kHistoryEntriesRemainingOrigin = url::Origin::Create(kURL1);
  const url::Origin kNoRemainingHistoryEntriesOrigin =
      url::Origin::Create(kURL3);
  const base::Time kAccessTime = base::Time::Now();
  clock()->SetNow(kAccessTime);
  service()->SetClockForTesting(clock());

  auto test_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestCookieName + "=1; max-age=3600", kAccessTime,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  // Record access for two top level origins for the same storage and cookie.
  service()->RecordCookieAccess({*test_cookie}, kHistoryEntriesRemainingOrigin);
  service()->RecordCookieAccess({*test_cookie},
                                kNoRemainingHistoryEntriesOrigin);
  service()->RecordStorageAPIAccess(kTestStorageOrigin, kTestStorageType,
                                    kHistoryEntriesRemainingOrigin);
  service()->RecordStorageAPIAccess(kTestStorageOrigin, kTestStorageType,
                                    kNoRemainingHistoryEntriesOrigin);

  // Ensure all records have been initially recorded.
  EXPECT_EQ(4u, GetAllAccessRecords().size());

  // Add history entries for all three URLs, then remove history entries for
  // URL1 and URL3. This will fire a history deletion event where the shared
  // origin of URL1 & URL2 has a remaining history entry, but no entry for the
  // URL3 origin remains.
  history_service()->AddPageWithDetails(kURL1, u"Test 1", 1, 1,
                                        base::Time::Now(), false,
                                        history::SOURCE_BROWSED);
  history_service()->AddPageWithDetails(kURL2, u"Test 2", 1, 1,
                                        base::Time::Now(), false,
                                        history::SOURCE_BROWSED);
  history_service()->AddPageWithDetails(kURL3, u"Test 3", 1, 1,
                                        base::Time::Now(), false,
                                        history::SOURCE_BROWSED);
  history_service()->DeleteURLs({kURL1, kURL3});
  base::RunLoop run_loop;
  history_service()->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  // Confirm that the records for the origin of URL3 have been removed, but the
  // records for the shared origin of URL1 & URL2 remain.
  auto records = GetAllAccessRecords();
  EXPECT_EQ(2u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kHistoryEntriesRemainingOrigin,
                            kAccessTime, records);
  CheckContainsStorageAPIRecord(kTestStorageOrigin, kTestStorageType,
                                kHistoryEntriesRemainingOrigin, kAccessTime,
                                records);
}

TEST_F(AccessContextAuditServiceTest, AllHistoryDeletion) {
  // Test that a deletion for all history removes all records, including those
  // for origins without any history entries.
  const GURL kHistoryEntryURL = GURL("https://history.com/test1");
  const url::Origin kHistoryEntryOrigin = url::Origin::Create(kHistoryEntryURL);
  const url::Origin kNoHistoryEntryOrigin =
      url::Origin::Create(GURL("https://no-history-entry.com/"));
  history_service()->AddPageWithDetails(kHistoryEntryURL, u"Test", 1, 1,
                                        base::Time::Now(), false,
                                        history::SOURCE_BROWSED);

  // Record two sets of unrelated accesses to cookies and storage APIs, one for
  // the origin with a history entry, and one for the origin without.
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(GURL("https://foo.com"),
                                     "foo=1; max-age=3600", base::Time::Now(),
                                     absl::nullopt /* server_time */,
                                     absl::nullopt /* cookie_partition_key */)},
      kHistoryEntryOrigin);
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(GURL("https://bar.com"),
                                     "bar=1; max-age=3600", base::Time::Now(),
                                     absl::nullopt /* server_time */,
                                     absl::nullopt /* cookie_partition_key */)},
      kNoHistoryEntryOrigin);
  service()->RecordStorageAPIAccess(
      url::Origin::Create(GURL("https://foo.com")),
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase,
      kHistoryEntryOrigin);
  service()->RecordStorageAPIAccess(
      url::Origin::Create(GURL("https://bar.com")),
      AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
      kNoHistoryEntryOrigin);

  // Check access has been initially recorded.
  EXPECT_EQ(4u, GetAllAccessRecords().size());

  // Expire all history and confirm that all records are removed.
  base::RunLoop run_loop;
  base::CancelableTaskTracker task_tracker;
  history_service()->ExpireHistoryBetween(
      std::set<GURL>(), base::Time(), base::Time(),
      /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  EXPECT_EQ(0u, GetAllAccessRecords().size());
}

TEST_F(AccessContextAuditServiceTest, TimeRangeHistoryDeletion) {
  // Test that deleting a time range of history records correctly removes
  // records within the time range, as well as records for which no history
  // entry for the top frame origin remains.

  // Create a situation where origin https://foo.com has history entries and
  // access records with timestamps both inside and outside the deleted range.
  // Additionally create a single history entry for origin https://bar.com
  // inside the deleted range, and multiple access records outside the range.
  // After deletion, the access records for https://foo.com outside the deletion
  // range should still be present, while all access records https://bar.com
  // should have been removed.

  const GURL kURL1 = GURL("https://foo.com/example.html");
  const GURL kURL2 = GURL("https://bar.com/another.html");
  const url::Origin kOrigin1 = url::Origin::Create(kURL1);
  const url::Origin kOrigin2 = url::Origin::Create(kURL2);
  const GURL kTestCookieURL = GURL("https://test.com");
  const auto kTestStorageType1 =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
  const auto kTestStorageType2 =
      AccessContextAuditDatabase::StorageAPIType::kIndexedDB;

  clock()->SetNow(base::Time::Now());
  service()->SetClockForTesting(clock());
  const base::Time kInsideTimeRange = clock()->Now() + base::Hours(1);
  const base::Time kOutsideTimeRange = clock()->Now() + base::Hours(3);

  history_service()->AddPageWithDetails(kURL1, u"Test1", 1, 1, kInsideTimeRange,
                                        false, history::SOURCE_BROWSED);
  history_service()->AddPageWithDetails(kURL2, u"Test2", 1, 1, kInsideTimeRange,
                                        false, history::SOURCE_BROWSED);
  history_service()->AddPageWithDetails(
      kURL1, u"Test3", 1, 1, kOutsideTimeRange, false, history::SOURCE_BROWSED);

  // Record accesses to cookies both inside and outside the deletion range.
  auto cookie_accessed_in_range = net::CanonicalCookie::Create(
      kTestCookieURL, "inside=1; max-age=3600", kInsideTimeRange,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie_accessed_outside_range = net::CanonicalCookie::Create(
      kTestCookieURL, "outside=1; max-age=3600", kOutsideTimeRange,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  clock()->SetNow(kInsideTimeRange);
  service()->RecordCookieAccess({*cookie_accessed_in_range}, kOrigin1);
  clock()->SetNow(kOutsideTimeRange);
  service()->RecordCookieAccess({*cookie_accessed_outside_range}, kOrigin1);
  service()->RecordCookieAccess({*cookie_accessed_outside_range}, kOrigin2);

  // Record accesses to storage APIs both inside and outside the deletion range.
  clock()->SetNow(kInsideTimeRange);
  service()->RecordStorageAPIAccess(kOrigin1, kTestStorageType1, kOrigin1);
  clock()->SetNow(kOutsideTimeRange);
  service()->RecordStorageAPIAccess(kOrigin1, kTestStorageType2, kOrigin1);
  service()->RecordStorageAPIAccess(kOrigin2, kTestStorageType1, kOrigin2);

  // Ensure all records have been initially recorded.
  EXPECT_EQ(6u, GetAllAccessRecords().size());

  // Expire history in target time range.
  base::RunLoop run_loop;
  base::CancelableTaskTracker task_tracker;
  history_service()->ExpireHistoryBetween(
      std::set<GURL>(), kInsideTimeRange - base::Minutes(10),
      kInsideTimeRange + base::Minutes(10),
      /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  // Ensure records have been removed as expected.
  auto records = GetAllAccessRecords();
  EXPECT_EQ(2u, records.size());
  CheckContainsCookieRecord(cookie_accessed_outside_range.get(), kOrigin1,
                            kOutsideTimeRange, records);
  CheckContainsStorageAPIRecord(kOrigin1, kTestStorageType2, kOrigin1,
                                kOutsideTimeRange, records);
}

TEST_F(AccessContextAuditServiceTest, SessionOnlyRecords) {
  // Check that data for cookie domains and storage origins are cleared on
  // service shutdown when the associated content settings indicate they should.
  const GURL kTestPersistentURL("https://persistent.com");
  const GURL kTestSessionOnlyExplicitURL("https://explicit-session-only.com");
  const GURL kTestSessionOnlyContentSettingURL("https://content-setting.com");
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  std::string kTestCookieName = "test";
  const auto kTestStorageType =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
  const base::Time kAccessTime = base::Time::Now();
  clock()->SetNow(kAccessTime);
  service()->SetClockForTesting(clock());

  // Create a cookie that will persist after shutdown.
  auto test_cookie_persistent = net::CanonicalCookie::Create(
      kTestPersistentURL, kTestCookieName + "=1; max-age=3600", kAccessTime,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  // Create a cookie that will persist (be cleared on next startup) because it
  // is explicitly session only.
  auto test_cookie_session_only_explicit = net::CanonicalCookie::Create(
      kTestSessionOnlyExplicitURL, kTestCookieName + "=1", kAccessTime,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  // Create a cookie that will be cleared because the content setting associated
  // with the cookie domain is set to session only.
  auto test_cookie_session_only_content_setting = net::CanonicalCookie::Create(
      kTestSessionOnlyContentSettingURL, kTestCookieName + "=1; max-age=3600",
      kAccessTime, absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  service()->RecordCookieAccess(
      {*test_cookie_persistent, *test_cookie_session_only_explicit,
       *test_cookie_session_only_content_setting},
      kTopFrameOrigin);

  // Record storage APIs for both persistent and content setting based session
  // only URLs.
  service()->RecordStorageAPIAccess(url::Origin::Create(kTestPersistentURL),
                                    kTestStorageType, kTopFrameOrigin);
  service()->RecordStorageAPIAccess(
      url::Origin::Create(kTestSessionOnlyContentSettingURL), kTestStorageType,
      kTopFrameOrigin);

  // Ensure all records have been initially recorded.
  EXPECT_EQ(5u, GetAllAccessRecords().size());

  // Apply Session Only exception.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(
          kTestSessionOnlyContentSettingURL, GURL(),
          ContentSettingsType::COOKIES,
          ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  // Instruct service to clear session only records and check that they are
  // correctly removed.
  service()->ClearSessionOnlyRecords();

  auto records = GetAllAccessRecords();
  ASSERT_EQ(3u, records.size());
  CheckContainsCookieRecord(test_cookie_persistent.get(), kTopFrameOrigin,
                            kAccessTime, records);
  CheckContainsCookieRecord(test_cookie_session_only_explicit.get(),
                            kTopFrameOrigin, kAccessTime, records);
  CheckContainsStorageAPIRecord(url::Origin::Create(GURL(kTestPersistentURL)),
                                kTestStorageType, kTopFrameOrigin, kAccessTime,
                                records);

  // Update the default content setting to SESSION_ONLY and ensure that all
  // records are cleared.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 ContentSetting::CONTENT_SETTING_SESSION_ONLY);
  service()->ClearSessionOnlyRecords();
  records = GetAllAccessRecords();
  ASSERT_EQ(0u, records.size());
}

TEST_F(AccessContextAuditServiceTest, OnStorageKeyDataCleared) {
  // Check that providing parameters with varying levels of specificity to the
  // OnStorageKeyDataCleared function all clear data correctly.
  auto kTopFrameOrigin = url::Origin::Create(GURL("https://example.com"));
  auto kTestOrigin1 = url::Origin::Create(GURL("https://test1.com"));
  auto kTestOrigin2 = url::Origin::Create(GURL("https://test2.com"));
  auto kTestOrigin3 = url::Origin::Create(GURL("https://test3.com"));

  const auto kTestStorageType1 =
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase;
  const auto kTestStorageType2 =
      AccessContextAuditDatabase::StorageAPIType::kIndexedDB;
  const auto kTestStorageType3 =
      AccessContextAuditDatabase::StorageAPIType::kCacheStorage;

  clock()->SetNow(base::Time());
  service()->SetClockForTesting(clock());

  clock()->Advance(base::Hours(1));
  service()->RecordStorageAPIAccess(kTestOrigin1, kTestStorageType1,
                                    kTopFrameOrigin);

  clock()->Advance(base::Hours(1));
  const base::Time kAccessTime1 = clock()->Now();
  service()->RecordStorageAPIAccess(kTestOrigin2, kTestStorageType2,
                                    kTopFrameOrigin);

  clock()->Advance(base::Hours(1));
  const base::Time kAccessTime2 = clock()->Now();
  service()->RecordStorageAPIAccess(kTestOrigin3, kTestStorageType3,
                                    kTopFrameOrigin);
  EXPECT_EQ(3U, GetAllAccessRecords().size());

  // Provide all parameters such that TestOrigin1's record is removed.
  auto storage_key_matcher =
      base::BindLambdaForTesting([&](const blink::StorageKey& storage_key) {
        return storage_key == blink::StorageKey(kTestOrigin1);
      });
  service()->OnStorageKeyDataCleared(
      content::StoragePartition::REMOVE_DATA_MASK_WEBSQL, storage_key_matcher,
      base::Time() + base::Minutes(50), base::Time() + base::Minutes(80));

  auto records = GetAllAccessRecords();
  ASSERT_EQ(2U, records.size());
  CheckContainsStorageAPIRecord(kTestOrigin2, kTestStorageType2,
                                kTopFrameOrigin, kAccessTime1, records);
  CheckContainsStorageAPIRecord(kTestOrigin3, kTestStorageType3,
                                kTopFrameOrigin, kAccessTime2, records);

  // Provide more generalised parameters that target TestOrigin2's record.
  service()->OnStorageKeyDataCleared(
      content::StoragePartition::REMOVE_DATA_MASK_ALL, base::NullCallback(),
      base::Time() + base::Minutes(80), base::Time() + base::Minutes(130));

  records = GetAllAccessRecords();
  ASSERT_EQ(1U, records.size());
  CheckContainsStorageAPIRecord(kTestOrigin3, kTestStorageType3,
                                kTopFrameOrigin, kAccessTime2, records);

  // Provide broadest possible parameters which should result in the final
  // record being removed.
  service()->OnStorageKeyDataCleared(
      content::StoragePartition::REMOVE_DATA_MASK_ALL, base::NullCallback(),
      base::Time(), base::Time::Max());

  records = GetAllAccessRecords();
  ASSERT_EQ(0U, records.size());
}

TEST_F(AccessContextAuditServiceTest, OpaqueOrigins) {
  // Check that records which have opaque top frame origins are not recorded.
  auto test_cookie = net::CanonicalCookie::Create(
      GURL("https://example.com"), "test_1=1; max-age=3600", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  service()->RecordCookieAccess({*test_cookie}, url::Origin());
  service()->RecordStorageAPIAccess(
      url::Origin::Create(GURL("https://example.com")),
      AccessContextAuditDatabase::StorageAPIType::kWebDatabase, url::Origin());

  auto records = GetAllAccessRecords();
  ASSERT_EQ(0U, records.size());
}

TEST_F(AccessContextAuditServiceTest, CookieAccessHelper) {
  // Check that the CookieAccessHelper is correctly forwarding accesses as
  // appropriate and responding to deletions.
  url::Origin kTopFrameOrigin = url::Origin::Create(GURL("https://test.com"));
  GURL kTestCookieURL("https://example.com");
  std::string kTestCookieName = "test";
  const base::Time kAccessTime1 = base::Time::Now();
  clock()->SetNow(kAccessTime1);
  service()->SetClockForTesting(clock());

  auto test_cookie = net::CanonicalCookie::Create(
      kTestCookieURL, kTestCookieName + "=1; max-age=3600", kAccessTime1,
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);

  // Record access to the cookie via a helper.
  auto helper = std::make_unique<AccessContextAuditService::CookieAccessHelper>(
      service());
  helper->RecordCookieAccess({*test_cookie}, kTopFrameOrigin);

  // Reaccess the cookie at a later time.
  const base::Time kAccessTime2 = clock()->Now() + base::Minutes(1);
  clock()->SetNow(kAccessTime2);
  helper->RecordCookieAccess({*test_cookie}, kTopFrameOrigin);

  // The only record should match the second access.
  auto records = GetAllAccessRecords();
  EXPECT_EQ(1u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin, kAccessTime2,
                            records);

  // Inform the audit service that the cookie has been deleted, which should
  // cause the helper to clear it from the set of accessed cookies and remove
  // the database record.
  service()->OnCookieChange(
      net::CookieChangeInfo(*test_cookie, net::CookieAccessResult(),
                            net::CookieChangeCause::EXPLICIT));

  // Flush the helper and ensure that no cookie access is recorded.
  helper->FlushCookieRecords();
  records = GetAllAccessRecords();
  EXPECT_EQ(0u, records.size());

  // Record a cookie access and delete the helper, the access should be flushed
  // to the service.
  const base::Time kAccessTime3 = clock()->Now() + base::Minutes(1);
  clock()->SetNow(kAccessTime3);
  helper->RecordCookieAccess({*test_cookie}, kTopFrameOrigin);

  helper.reset();
  records = GetAllAccessRecords();
  EXPECT_EQ(1u, records.size());
  CheckContainsCookieRecord(test_cookie.get(), kTopFrameOrigin, kAccessTime3,
                            records);
}

class AccessContextAuditThirdPartyDataClearingTest
    : public AccessContextAuditServiceTest {
 protected:
  void InsertAccessRecords(GURL top_level_url, GURL cross_site_url) {
    url::Origin top_level_origin = url::Origin::Create(top_level_url);
    url::Origin cross_site_origin = url::Origin::Create(cross_site_url);

    // Should keep records of same-site and cross-site cookie accesses.
    // Third-party data clearing does not depend on the access context auditing
    // service to determine which cookies to clear, so these should get deleted.
    service()->RecordCookieAccess(
        {*net::CanonicalCookie::Create(
            top_level_url, "same=site; max-age=3600", base::Time::Now(),
            absl::nullopt /* server_time */,
            absl::nullopt /* cookie_partition_key */)},
        top_level_origin);
    service()->RecordCookieAccess(
        {*net::CanonicalCookie::Create(
            cross_site_url, "cross=site; max-age=3600", base::Time::Now(),
            absl::nullopt /* server_time */,
            absl::nullopt /* cookie_partition_key */)},
        top_level_origin);

    // Set a same-site storage access record. This should get deleted.
    service()->RecordStorageAPIAccess(
        top_level_origin,
        AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
        top_level_origin);
    // Set a cross-site storage access record. This should be kept but the
    // top-level origin should be removed.
    service()->RecordStorageAPIAccess(
        cross_site_origin,
        AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
        top_level_origin);

    // Ensure all records are added.
    EXPECT_EQ(4u, GetAllAccessRecords().size());
  }

  void ValidateCrossSiteStorageRecords(GURL cross_site_url) {
    // Ensure only the cross-site storage access record remains and its
    // top-level origin is opaque.
    std::vector<AccessContextAuditDatabase::AccessRecord> records =
        GetAllAccessRecords();
    EXPECT_EQ(1u, records.size());
    EXPECT_EQ(AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
              records[0].type);
    EXPECT_EQ(url::Origin::Create(cross_site_url), records[0].origin);
    EXPECT_TRUE(records[0].top_frame_origin.opaque());
  }

  std::vector<base::test::FeatureRef> enabled_features() override {
    return {features::kClientStorageAccessContextAuditing,
            browsing_data::features::kEnableRemovingAllThirdPartyCookies};
  }
  std::vector<base::test::FeatureRef> disabled_features() override {
    return {};
  }
};

// Test that when we enable user controls to clear third-party data, we do not
// clear records of cross-site storage access. This is because when we delete
// third-party data, we query the access context audit service to determine
// which sites accessed storage in cross-site contexts and delete storage for
// those sites. Our solution is to remove the top-level origin from the records
// when users clear history, but only clear the records when the respective
// storage is deleted.
TEST_F(AccessContextAuditThirdPartyDataClearingTest, HistoryDeletion) {
  const GURL kTopLevelURL("https://toplevel.com/");
  const GURL kCrossSiteURL("https://cross.site.com/");

  InsertAccessRecords(kTopLevelURL, kCrossSiteURL);

  // Remove history entries for the top level URL.
  history_service()->AddPageWithDetails(kTopLevelURL, u"Test 1", 1, 1,
                                        base::Time::Now(), false,
                                        history::SOURCE_BROWSED);
  history_service()->DeleteURLs({kTopLevelURL});
  base::RunLoop run_loop;
  history_service()->FlushForTest(run_loop.QuitClosure());
  run_loop.Run();

  ValidateCrossSiteStorageRecords(kCrossSiteURL);
}

TEST_F(AccessContextAuditThirdPartyDataClearingTest, AllHistoryDeletion) {
  const GURL kTopLevelURL("https://toplevel.com/");
  const GURL kCrossSiteURL("https://cross.site.com/");

  InsertAccessRecords(kTopLevelURL, kCrossSiteURL);

  // Expire all history and confirm that all records are removed.
  base::RunLoop run_loop;
  base::CancelableTaskTracker task_tracker;
  history_service()->ExpireHistoryBetween(
      std::set<GURL>(), base::Time(), base::Time(),
      /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  ValidateCrossSiteStorageRecords(kCrossSiteURL);
}

TEST_F(AccessContextAuditThirdPartyDataClearingTest, TimeRangeHistoryDeletion) {
  const GURL kTopLevelURL("https://toplevel.com/");
  const GURL kInsideTimeRangeURL("https://inside.range.com/");
  const GURL kOutsideTimeRangeURL("https://outside.range.com/");

  const url::Origin kTopLevelOrigin = url::Origin::Create(kTopLevelURL);
  const url::Origin kInsideTimeRangeOrigin =
      url::Origin::Create(kInsideTimeRangeURL);
  const url::Origin kOutsideTimeRangeOrigin =
      url::Origin::Create(kOutsideTimeRangeURL);

  clock()->SetNow(base::Time::Now());
  service()->SetClockForTesting(clock());
  const base::Time kInsideTimeRange = clock()->Now() + base::Hours(1);
  const base::Time kOutsideTimeRange = clock()->Now() + base::Hours(3);

  clock()->SetNow(kOutsideTimeRange);
  // A cookie record outside the time range should not be modified.
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(
          kOutsideTimeRangeURL, "same=site; max-age=3600", kOutsideTimeRange,
          absl::nullopt /* server_time */,
          absl::nullopt /* cookie_partition_key */)},
      kTopLevelOrigin);
  clock()->SetNow(kInsideTimeRange);
  // A cookie record inside the time range should be deleted.
  service()->RecordCookieAccess(
      {*net::CanonicalCookie::Create(
          kInsideTimeRangeURL, "cross=site; max-age=3600", kInsideTimeRange,
          absl::nullopt /* server_time */,
          absl::nullopt /* cookie_partition_key */)},
      kTopLevelOrigin);
  // A same-site storage record in the time range should be deleted.
  service()->RecordStorageAPIAccess(
      kTopLevelOrigin, AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
      kTopLevelOrigin);
  // Set a cross-site storage access record in the time range. This should be
  // kept but the top-level origin should be removed.
  service()->RecordStorageAPIAccess(
      kInsideTimeRangeOrigin,
      AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
      kTopLevelOrigin);
  clock()->SetNow(kOutsideTimeRange);
  // A cross-site storage record outside the time range should not be modified.
  service()->RecordStorageAPIAccess(
      kOutsideTimeRangeOrigin,
      AccessContextAuditDatabase::StorageAPIType::kServiceWorker,
      kTopLevelOrigin);
  EXPECT_EQ(5u, GetAllAccessRecords().size());

  history_service()->AddPageWithDetails(kTopLevelURL, u"Test1", 1, 1,
                                        kInsideTimeRange, false,
                                        history::SOURCE_BROWSED);
  history_service()->AddPageWithDetails(kTopLevelURL, u"Test1", 1, 1,
                                        kOutsideTimeRange, false,
                                        history::SOURCE_BROWSED);

  // Expire history in target time range.
  base::RunLoop run_loop;
  base::CancelableTaskTracker task_tracker;
  history_service()->ExpireHistoryBetween(
      std::set<GURL>(), kInsideTimeRange - base::Minutes(10),
      kInsideTimeRange + base::Minutes(10),
      /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  std::vector<AccessContextAuditDatabase::AccessRecord> records =
      GetAllAccessRecords();
  EXPECT_EQ(3u, records.size());
  size_t n_storage_records = 0;
  for (const auto& record : records) {
    if (record.type == AccessContextAuditDatabase::StorageAPIType::kCookie) {
      EXPECT_EQ("outside.range.com", record.domain);
      continue;
    }
    n_storage_records++;
    EXPECT_EQ(record.origin == url::Origin::Create(kInsideTimeRangeURL),
              record.top_frame_origin.opaque());
  }
  EXPECT_EQ(2u, n_storage_records);
}
