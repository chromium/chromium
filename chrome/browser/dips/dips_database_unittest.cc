// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/dips/dips_database.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::Time;

class DIPSDatabase;

namespace {
class TestDatabase : public DIPSDatabase {
 public:
  explicit TestDatabase(const absl::optional<base::FilePath>& db_path)
      : DIPSDatabase(db_path) {}
  void ComputeDatabaseMetricsForTesting() { ComputeDatabaseMetrics(); }
};

enum ColumnType {
  kSiteStorage,
  kUserInteraction,
  kStatefulBounce,
  kStatelessBounce
};
}  // namespace
class DIPSDatabaseTest : public testing::Test {
 public:
  explicit DIPSDatabaseTest(bool in_memory) : in_memory_(in_memory) {}

 protected:
  std::unique_ptr<TestDatabase> db_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  // Test setup.
  void SetUp() override {
    if (in_memory_) {
      db_ = std::make_unique<TestDatabase>(absl::nullopt);
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      db_path_ = temp_dir_.GetPath().AppendASCII("DIPS.db");
      db_ = std::make_unique<TestDatabase>(db_path_);
    }

    ASSERT_TRUE(db_->CheckDBInit());
  }

  void TearDown() override {
    db_.reset();

    // Deletes temporary directory from on-disk tests
    if (!in_memory_)
      ASSERT_TRUE(temp_dir_.Delete());
  }

 private:
  bool in_memory_;
};

// A test class that lets us ensure that we can add, read, update, and delete
// bounces for all columns in the DIPSDatabase. Parameterized over whether the
// db is in memory, and what column we're testing.
class DIPSDatabaseAllColumnTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<std::tuple<bool, ColumnType>> {
 public:
  DIPSDatabaseAllColumnTest()
      : DIPSDatabaseTest(std::get<0>(GetParam())),
        column_(std::get<1>(GetParam())) {}

 protected:
  // Uses `times` to write  to the first and last columns for `column_` in the
  // `site` row in `db`. This also writes the empty time stamps to all other
  // columns in `db`
  bool WriteToVariableColumn(const std::string& site,
                             const TimestampRange& times) {
    return db_->Write(site, column_ == kSiteStorage ? times : TimestampRange(),
                      column_ == kUserInteraction ? times : TimestampRange(),
                      column_ == kStatefulBounce ? times : TimestampRange(),
                      column_ == kStatelessBounce ? times : TimestampRange());
  }

  TimestampRange ReadValueForVariableColumn(absl::optional<StateValue> value) {
    switch (column_) {
      case ColumnType::kSiteStorage:
        return value->site_storage_times;
      case ColumnType::kUserInteraction:
        return value->user_interaction_times;
      case ColumnType::kStatefulBounce:
        return value->stateful_bounce_times;
      case ColumnType::kStatelessBounce:
        return value->stateless_bounce_times;
    }
  }

 private:
  ColumnType column_;
};

// Test adding entries in the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, AddBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce_1{Time::FromDoubleT(1), Time::FromDoubleT(1)};
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));
  // Verify that site is in `bounces` using Read().
  EXPECT_TRUE(db_->Read(site).has_value());
}

// Test updating entries in the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, UpdateBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce_1{Time::FromDoubleT(1), Time::FromDoubleT(1)};
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));

  // Verify that site's entry in `bounces` is now at t = 1
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_1);

  // Update site's entry with a bounce at t = 2
  TimestampRange bounce_2{Time::FromDoubleT(2), Time::FromDoubleT(3)};
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_2));

  // Verify that site's entry in `bounces` is now at t = 2
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_2);
}

// Test deleting an entry from the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, DeleteBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce{Time::FromDoubleT(1), Time::FromDoubleT(1)};
  EXPECT_TRUE(WriteToVariableColumn(site, bounce));

  // Verify that site has state tracked in bounces.
  EXPECT_TRUE(db_->Read(site).has_value());

  //  Delete site's entry in bounces.
  EXPECT_TRUE(db_->RemoveRow(site));

  // Query the bounces for site, making sure there is no state now.
  EXPECT_FALSE(db_->Read(site).has_value());
}

// Test reading the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, ReadBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("https://example.test"));

  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce));
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce);

  // Query a site that never had DIPS State, verifying that is has no entry.
  EXPECT_FALSE(db_->Read(GetSiteForDIPS(GURL("https://www.not-in-db.com/")))
                   .has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DIPSDatabaseAllColumnTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(ColumnType::kSiteStorage,
                                         ColumnType::kUserInteraction,
                                         ColumnType::kStatefulBounce,
                                         ColumnType::kStatelessBounce)));

// A test class that verifies the behavior of the methods used to query the
// DIPSDatabase for information more efficiently than using DIPSDatabase::Read.
//
// Parameterized over whether the db is in memory.
class DIPSDatabaseQueryTest : public DIPSDatabaseTest,
                              public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseQueryTest() : DIPSDatabaseTest(GetParam()) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();

    DCHECK(db_);
    // Add entries to the database filling in the columns we want to test.
    // These test entries correspond with the following cases:
    // - a site accesses the browser's storage only
    // - a site redirects the user while accessing storage
    // - a site redirects the user (without regard to storage access)
    //  All of these entries include a user interaction.
    db_->Write("https://storage-only.test", storage_times, interaction_times,
               /*stateful_bounce_times=*/{}, /*stateless_bounce_times=*/{});
    db_->Write("https://stateful-bounce.test", storage_times, interaction_times,
               stateful_bounce_times,
               /*stateless_bounce_times=*/{});
    db_->Write("https://stateless-bounce.test",
               /*storage_times=*/{}, interaction_times,
               /*stateful_bounce_times=*/{}, stateless_bounce_times);
  }

 protected:
  // Rewrites the entries that were wrote in SetUp() to not have interactions.
  void ClearAllInteractions() {
    db_->Write("https://storage-only.test", storage_times,
               /*interaction_times=*/{},
               /*stateful_bounce_times=*/{}, /*stateless_bounce_times=*/{});
    db_->Write("https://stateful-bounce.test", storage_times,
               /*interaction_times=*/{}, stateful_bounce_times,
               /*stateless_bounce_times=*/{});
    db_->Write("https://stateless-bounce.test",
               /*storage_times=*/{}, /*interaction_times=*/{},
               /*stateful_bounce_times=*/{}, stateless_bounce_times);
  }
  // For ease of testings if a site has an entry in its `user_interaction`
  // column the timestamp is at t=1 and so on.
  base::Time interaction = Time::FromDoubleT(1);
  base::Time storage = Time::FromDoubleT(2);
  base::Time stateful_bounce = Time::FromDoubleT(3);
  base::Time stateless_bounce = Time::FromDoubleT(4);

  // Extra times used for testing querying at various times before or after the
  // events being recorded to the db.
  base::Time before_interaction = Time::FromDoubleT(0.9999);
  base::Time after_interaction = Time::FromDoubleT(1.0001);
  base::Time before_storage = Time::FromDoubleT(1.9999);
  base::Time after_storage = Time::FromDoubleT(2.0001);
  base::Time before_stateful_bounce = Time::FromDoubleT(2.9999);
  base::Time after_stateful_bounce = Time::FromDoubleT(3.0001);
  base::Time after_stateless_bounce = Time::FromDoubleT(4.0001);

  TimestampRange interaction_times = {interaction, interaction};
  TimestampRange storage_times = {storage, storage};
  TimestampRange stateful_bounce_times = {stateful_bounce, stateful_bounce};
  TimestampRange stateless_bounce_times = {stateless_bounce, stateless_bounce};
};

TEST_P(DIPSDatabaseQueryTest, VerifyInteractionsNonNull) {
  ClearAllInteractions();
  EXPECT_THAT(db_->GetSitesThatBounced(before_storage, interaction),
              testing::IsEmpty());
  EXPECT_THAT(db_->GetSitesThatUsedStorage(before_storage, interaction),
              testing::IsEmpty());
  EXPECT_THAT(db_->GetSitesThatBouncedWithState(before_storage, interaction),
              testing::IsEmpty());
}

TEST_P(DIPSDatabaseQueryTest, EnsureLastInteractionStrictlyBeforeRangeStart) {
  // Verify that the |last_interaction| shouldn't be greater than |range_start|
  // for each query method.
  base::Time range_start = Time::FromDoubleT(0);
  base::Time last_interaction = Time::FromDoubleT(1);

  EXPECT_DCHECK_DEATH(db_->GetSitesThatBounced(range_start, last_interaction));
  EXPECT_DCHECK_DEATH(
      db_->GetSitesThatBouncedWithState(range_start, last_interaction));
  EXPECT_DCHECK_DEATH(
      db_->GetSitesThatUsedStorage(range_start, last_interaction));

  // Verify that the |last_interaction| should be strictly less than
  // |range_start| for each query method.
  EXPECT_DCHECK_DEATH(
      db_->GetSitesThatBounced(range_start, /*last_interaction=*/range_start));
  EXPECT_DCHECK_DEATH(db_->GetSitesThatBouncedWithState(
      range_start, /*last_interaction=*/range_start));
  EXPECT_DCHECK_DEATH(db_->GetSitesThatUsedStorage(
      range_start, /*last_interaction=*/range_start));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBounced_InteractionTest) {
  // All queries should be for the entire range (range_start > before_storage)
  // so that we can test the behavior of this method for different values of
  // last_interaction.
  base::Time earliest_range_start = before_storage;

  // No sites are expected since all of their interactions happen later on.
  EXPECT_THAT(
      db_->GetSitesThatBounced(earliest_range_start, before_interaction),
      testing::IsEmpty());

  // No sites are expected since the last_interaction_time check is strictly
  // less, and not less than or equal to.
  EXPECT_THAT(db_->GetSitesThatBounced(earliest_range_start, interaction),
              testing::IsEmpty());

  // When the last_interaction bound is `after_interaction`, both sites that
  // bounced are returned since they had user interaction before it.
  EXPECT_THAT(db_->GetSitesThatBounced(earliest_range_start, after_interaction),
              testing::ElementsAre("https://stateful-bounce.test",
                                   "https://stateless-bounce.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBounced_RangeStartTest) {
  EXPECT_THAT(db_->GetSitesThatBounced(before_storage, after_interaction),
              testing::ElementsAre("https://stateful-bounce.test",
                                   "https://stateless-bounce.test"));
  // When the range begins after the stateful bounce happened,
  // "stateless-bounce.test" is returned since it bounces later.
  EXPECT_THAT(
      db_->GetSitesThatBounced(after_stateful_bounce, after_interaction),
      testing::ElementsAre("https://stateless-bounce.test"));
  // When the range begins after the stateless bounce happened, neither are
  // returned since both sites bounced before this.
  EXPECT_THAT(
      db_->GetSitesThatBounced(after_stateless_bounce, after_interaction),
      testing::IsEmpty());
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBouncedWithState_InteractionTest) {
  // All queries should be for the entire range (range_start > before_storage)
  // so that we can test the behavior of this method for different values of
  // last_interaction.
  base::Time earliest_range_start = before_storage;

  // No sites are expected since all of their interactions happen later on.
  EXPECT_THAT(db_->GetSitesThatBouncedWithState(earliest_range_start,
                                                before_interaction),
              testing::IsEmpty());

  // No sites are expected since the last_interaction_time check is strictly
  // less, and not less than or equal to.
  EXPECT_THAT(
      db_->GetSitesThatBouncedWithState(earliest_range_start, interaction),
      testing::IsEmpty());

  // When the last_interaction bound is `after_interaction`, the site that
  // did a stateful bounce is returned since it had user interaction before it.
  EXPECT_THAT(db_->GetSitesThatBouncedWithState(earliest_range_start,
                                                after_interaction),
              testing::ElementsAre("https://stateful-bounce.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBouncedWithState_RangeStartTest) {
  EXPECT_THAT(
      db_->GetSitesThatBouncedWithState(before_storage, after_interaction),
      testing::ElementsAre("https://stateful-bounce.test"));
  // When the range begins after the stateful bounce happened, no other sites
  // are returned (since no other site did a stateful bounce after this time).
  EXPECT_THAT(db_->GetSitesThatBouncedWithState(after_stateful_bounce,
                                                after_interaction),
              testing::IsEmpty());
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatUsedStorage_InteractionTest) {
  // All queries should be for the entire range (range_start > before_storage)
  // so that we can test the behavior of this method for different values of
  // last_interaction.
  base::Time earliest_range_start = before_storage;

  // No sites are expected since all of their interactions happen later on.
  EXPECT_THAT(
      db_->GetSitesThatUsedStorage(earliest_range_start, before_interaction),
      testing::IsEmpty());

  // No sites are expected since the last_interaction_time check is strictly
  // less, and not less than or equal to.
  EXPECT_THAT(db_->GetSitesThatUsedStorage(earliest_range_start, interaction),
              testing::IsEmpty());

  // When the last_interaction bound is `after_interaction`, both sites that
  // used storage are returned since they had user interaction before it.
  EXPECT_THAT(
      db_->GetSitesThatUsedStorage(earliest_range_start, after_interaction),
      testing::ElementsAre("https://stateful-bounce.test",
                           "https://storage-only.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatUsedStorage_RangeStartTest) {
  // When the range begins at 0, both sites that used storage are returned since
  // they did so after t=0.
  EXPECT_THAT(db_->GetSitesThatUsedStorage(before_storage, after_interaction),
              testing::ElementsAre("https://stateful-bounce.test",
                                   "https://storage-only.test"));
  // When the range begins after "storage-only.test" used storage, only
  // "stateful_bounce.test" is returned since it uses storage later.
  EXPECT_THAT(db_->GetSitesThatUsedStorage(after_storage, after_interaction),
              testing::ElementsAre("https://stateful-bounce.test"));
  // When the range begins after the stateful bounce happened, no other sites
  // are returned (since no other site used storage after this time).
  EXPECT_THAT(
      db_->GetSitesThatUsedStorage(after_stateful_bounce, after_interaction),
      testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, DIPSDatabaseQueryTest, ::testing::Bool());

// A test class that verifies DIPSDatabase garbage collection behavior.
class DIPSDatabaseGarbageCollectionTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseGarbageCollectionTest() : DIPSDatabaseTest(true) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();

    DCHECK(db_);

    db_->SetMaxEntriesForTesting(200);
    db_->SetPurgeEntriesForTesting(20);

    recent_interaction = Time::Now();
    old_interaction = Time::Now() - DIPSDatabase::kMaxAge - base::Days(1);

    recent_interaction_times = {recent_interaction, recent_interaction};
    old_interaction_times = {old_interaction, old_interaction};
  }

  void BloatBouncesForGC(int num_recent_entries, int num_old_entries) {
    DCHECK(db_);

    for (int i = 0; i < num_recent_entries; i++) {
      db_->Write(base::StrCat({"https://recent_interaction.test",
                               base::NumberToString(i)}),
                 storage_times, recent_interaction_times, stateful_bounce_times,
                 stateless_bounce_times);
    }

    for (int i = 0; i < num_old_entries; i++) {
      db_->Write(base::StrCat(
                     {"https://old_interaction.test", base::NumberToString(i)}),
                 storage_times, old_interaction_times, stateful_bounce_times,
                 stateless_bounce_times);
    }
  }

  base::Time recent_interaction;
  base::Time old_interaction;
  base::Time storage = Time::FromDoubleT(2);
  base::Time stateful_bounce = Time::FromDoubleT(3);
  base::Time stateless_bounce = Time::FromDoubleT(4);

  TimestampRange recent_interaction_times;
  TimestampRange old_interaction_times;
  TimestampRange storage_times = {storage, storage};
  TimestampRange stateful_bounce_times = {stateful_bounce, stateful_bounce};
  TimestampRange stateless_bounce_times = {stateless_bounce, stateless_bounce};
};

// More than |max_entries_| entries with recent user interaction; garbage
// collection should purge down to |max_entries_| - |purge_entries_| entries.
TEST_P(DIPSDatabaseGarbageCollectionTest, RemovesRecentOverMax) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() * 2,
                    /*num_old_entries=*/0);

  EXPECT_EQ(db_->GarbageCollect(),
            db_->GetMaxEntries() + db_->GetPurgeEntries());

  EXPECT_EQ(db_->GetEntryCount(),
            db_->GetMaxEntries() - db_->GetPurgeEntries());
}

// Less than |max_entries_| entries, some with expired user interaction and some
// with recent; no entries should be garbage collected.
TEST_P(DIPSDatabaseGarbageCollectionTest, PreservesUnderMax) {
  BloatBouncesForGC(
      /*num_recent_entries=*/(db_->GetMaxEntries() - db_->GetPurgeEntries()) /
          4,
      /*num_old_entries=*/(db_->GetMaxEntries() - db_->GetPurgeEntries()) / 4);

  EXPECT_EQ(db_->GarbageCollect(), static_cast<size_t>(0));

  EXPECT_EQ(db_->GetEntryCount(),
            (db_->GetMaxEntries() - db_->GetPurgeEntries()) / 2);
}

// Exactly |max_entries_| entries, some with expired user interaction and some
// with recent; no entries should be garbage collected.
TEST_P(DIPSDatabaseGarbageCollectionTest, PreservesMax) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() / 2,
                    /*num_old_entries=*/db_->GetMaxEntries() / 2);

  EXPECT_EQ(db_->GarbageCollect(), static_cast<size_t>(0));

  EXPECT_EQ(db_->GetEntryCount(), db_->GetMaxEntries());
}

// More than |max_entries_| entries with recent user interaction and a few with
// expired user interaction; only entries with expired user interaction should
// be garbage collected by pure expiration.
TEST_P(DIPSDatabaseGarbageCollectionTest, ExpirationPreservesRecent) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() * 2,
                    /*num_old_entries=*/db_->GetMaxEntries() / 2);

  EXPECT_EQ(db_->GarbageCollectExpired(), db_->GetMaxEntries() / 2);

  EXPECT_EQ(db_->GetEntryCount(), db_->GetMaxEntries() * 2);
}

// The entries with the oldest interaction and storage times should be deleted
// first.
TEST_P(DIPSDatabaseGarbageCollectionTest, OldestEntriesRemoved) {
  db_->Write("https://old_interaction.test", {},
             /*interaction_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             {}, {});
  db_->Write("https://old_storage_old_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             /*interaction_times=*/{Time::FromDoubleT(2), Time::FromDoubleT(2)},
             {}, {});
  db_->Write("https://old_storage.test",
             /*storage_times=*/{Time::FromDoubleT(3), Time::FromDoubleT(3)}, {},
             {}, {});
  db_->Write("https://old_storage_new_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             /*interaction_times=*/{Time::FromDoubleT(4), Time::FromDoubleT(4)},
             {}, {});
  db_->Write("https://new_storage_old_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(5), Time::FromDoubleT(5)},
             /*interaction_times=*/{Time::FromDoubleT(2), Time::FromDoubleT(2)},
             {}, {});
  db_->Write("https://new_storage_new_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(6), Time::FromDoubleT(6)},
             /*interaction_times=*/{Time::FromDoubleT(7), Time::FromDoubleT(7)},
             {}, {});

  EXPECT_EQ(db_->GarbageCollectOldest(3), static_cast<size_t>(3));
  EXPECT_EQ(db_->GetEntryCount(), static_cast<size_t>(3));

  EXPECT_THAT(db_->GetAllSitesForTesting(),
              testing::ElementsAre("https://new_storage_new_interaction.test",
                                   "https://new_storage_old_interaction.test",
                                   "https://old_storage_new_interaction.test"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSDatabaseGarbageCollectionTest,
                         ::testing::Bool());

// A test class that verifies DIPSDatabase database health metrics collection
// behavior. Created on-disk so opening a corrupt database file can be tested.
class DIPSDatabaseHistogramTest : public DIPSDatabaseTest {
 public:
  DIPSDatabaseHistogramTest() : DIPSDatabaseTest(false) {}

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(DIPSDatabaseHistogramTest, HealthMetrics) {
  // The database was initialized successfully.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseErrors", 0);
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseInit", 1);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseInit", 1, 1);

  // These should each have one sample after database initialization.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseHealthMetricsTime", 1);
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseSize", 1);

  // The database should be empty.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseEntryCount", 1);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseEntryCount", 0, 1);

  // Write an entry to the db.
  db_->Write("https://url1.test", {},
             /*interaction_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             {}, {});
  db_->ComputeDatabaseMetricsForTesting();

  // These should be unchanged.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseErrors", 0);
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseInit", 1);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseInit", 1, 1);

  // These should each have two samples now.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseHealthMetricsTime", 2);
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseSize", 2);

  // The database should now have one entry.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseEntryCount", 2);
  histograms().ExpectBucketCount("Privacy.DIPS.DatabaseEntryCount", 1, 1);
}

TEST_F(DIPSDatabaseHistogramTest, ErrorMetrics) {
  // The database was initialized successfully.
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseErrors", 0);
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseInit", 1);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseInit", 1, 1);

  // Write an entry to the db.
  db_->Write("https://url1.test", {},
             /*interaction_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             {}, {});
  EXPECT_EQ(db_->GetEntryCount(), static_cast<size_t>(1));

  // Corrupt the database.
  db_.reset();
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path_));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(db_ = std::make_unique<TestDatabase>(db_path_));
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseInit", 2);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseInit", 1, 2);

  // No data should be present as the database should have been razed.
  EXPECT_EQ(db_->GetEntryCount(), static_cast<size_t>(0));

  // Verify that the corruption error was reported.
  EXPECT_TRUE(expecter.SawExpectedErrors());
  histograms().ExpectTotalCount("Privacy.DIPS.DatabaseErrors", 1);
  histograms().ExpectUniqueSample("Privacy.DIPS.DatabaseErrors",
                                  sql::SqliteLoggedResultCode::kCorrupt, 1);
}
