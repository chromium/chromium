// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::Time;

class DIPSDatabase;

namespace {

int kCurrentVersionNumber = 2;

class TestDatabase : public DIPSDatabase {
 public:
  explicit TestDatabase(const absl::optional<base::FilePath>& db_path)
      : DIPSDatabase(db_path) {}
  void ComputeDatabaseMetricsForTesting() { ComputeDatabaseMetrics(); }
};

enum ColumnType { kSiteStorage, kUserInteraction, kStatefulBounce, kBounce };
}  // namespace
class DIPSDatabaseTest : public testing::Test {
 public:
  explicit DIPSDatabaseTest(bool in_memory) : in_memory_(in_memory) {}

 protected:
  base::SimpleTestClock clock_;
  std::unique_ptr<TestDatabase> db_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  base::test::ScopedFeatureList features_;
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
    db_->SetClockForTesting(clock());
  }

  void TearDown() override {
    db_.reset();

    // Deletes temporary directory from on-disk tests
    if (!in_memory_)
      ASSERT_TRUE(temp_dir_.Delete());
  }

  base::Time Now() { return clock_.Now(); }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }
  void AdvanceTimeBy(base::TimeDelta delta) { clock_.Advance(delta); }

  base::Clock* clock() { return &clock_; }

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

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    // Use inf ttl to prevent interactions from expiring unintentionally.
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});
  }

 protected:
  bool IsBounce(ColumnType column) {
    return column == kBounce || column == kStatefulBounce;
  }

  // Uses `times` to write  to the first and last columns for `column_` in the
  // `site` row in `db`. This also writes the empty time stamps to all other
  // columns in `db` that are unrelated.
  bool WriteToVariableColumn(const std::string& site,
                             const TimestampRange& times) {
    return db_->Write(site, column_ == kSiteStorage ? times : TimestampRange(),
                      column_ == kUserInteraction ? times : TimestampRange(),
                      column_ == kStatefulBounce ? times : TimestampRange(),
                      IsBounce(column_) ? times : TimestampRange());
  }

  TimestampRange ReadValueForVariableColumn(absl::optional<StateValue> value) {
    switch (column_) {
      case ColumnType::kSiteStorage:
        return value->site_storage_times;
      case ColumnType::kUserInteraction:
        return value->user_interaction_times;
      case ColumnType::kStatefulBounce:
        return value->stateful_bounce_times;
      case ColumnType::kBounce:
        return value->bounce_times;
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

// Test deleting many entries from the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, DeleteSeveralBounces) {
  // Add a bounce for site.
  const std::string site1 = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string site2 = GetSiteForDIPS(GURL("http://www.picasa.com/"));

  TimestampRange bounce{Time::FromDoubleT(1), Time::FromDoubleT(1)};
  EXPECT_TRUE(WriteToVariableColumn(site1, bounce));
  EXPECT_TRUE(WriteToVariableColumn(site2, bounce));

  // Verify that both sites are in the bounces table.
  EXPECT_TRUE(db_->Read(site1).has_value());
  EXPECT_TRUE(db_->Read(site2).has_value());

  //  Delete site's entry in bounces.
  EXPECT_TRUE(db_->RemoveRows({site1, site2}));

  // Query the bounces for site, making sure there is no state now.
  EXPECT_FALSE(db_->Read(site1).has_value());
  EXPECT_FALSE(db_->Read(site2).has_value());
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
                                         ColumnType::kBounce)));

// A test class that verifies the behavior DIPSDatabase with respect to
// interactions.
//
// Parameterized over whether the db is in memory.
class DIPSDatabaseInteractionTest : public DIPSDatabaseTest,
                                    public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseInteractionTest() : DIPSDatabaseTest(GetParam()) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    DCHECK(db_);
    db_->Write("storage-only.test", storage_times,
               {interaction_for_storage, interaction_for_storage},
               /*stateful_bounce_times=*/{}, /*bounce_times=*/{});
    db_->Write(
        "stateful-bounce.test", stateful_bounce_times,
        {interaction_for_stateful_bounce, interaction_for_stateful_bounce},
        stateful_bounce_times,
        /*bounce_times=*/stateful_bounce_times);
    db_->Write(
        "stateless-bounce.test",
        /*storage_times=*/{},
        {interaction_for_stateless_bounce, interaction_for_stateless_bounce},
        /*stateful_bounce_times=*/{}, bounce_times);
  }

 protected:
  // Used to simulate just before/after another timestamp.
  base::TimeDelta tiny_delta = base::Milliseconds(1);

  base::Time storage = Time::FromDoubleT(1);
  base::Time interaction_for_storage = Time::FromDoubleT(2);

  base::Time stateful_bounce = Time::FromDoubleT(3);
  base::Time interaction_for_stateful_bounce = Time::FromDoubleT(5);

  base::Time stateless_bounce = Time::FromDoubleT(6);
  base::Time interaction_for_stateless_bounce = Time::FromDoubleT(9);

  TimestampRange storage_times = {storage, storage};
  TimestampRange stateful_bounce_times = {stateful_bounce, stateful_bounce};
  TimestampRange bounce_times = {stateless_bounce, stateless_bounce};
};

TEST_P(DIPSDatabaseInteractionTest, ClearExpiredInteractions) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(dips::kFeature,
                                              {{"interaction_ttl", "3s"}});

  base::TimeDelta interaction_ttl = dips::kInteractionTtl.Get();

  ASSERT_EQ(interaction_ttl, base::Seconds(3));

  EXPECT_THAT(
      db_->GetAllSitesForTesting(),
      testing::ElementsAre("stateful-bounce.test", "stateless-bounce.test",
                           "storage-only.test"));
  AdvanceTimeTo(interaction_for_storage + interaction_ttl + tiny_delta);
  EXPECT_EQ(db_->ClearRowsWithExpiredInteractions(), 1u);
  EXPECT_THAT(
      db_->GetAllSitesForTesting(),
      testing::ElementsAre("stateful-bounce.test", "stateless-bounce.test"));

  AdvanceTimeTo(interaction_for_stateful_bounce + interaction_ttl + tiny_delta);
  EXPECT_EQ(db_->ClearRowsWithExpiredInteractions(), 1u);
  EXPECT_THAT(db_->GetAllSitesForTesting(),
              testing::ElementsAre("stateless-bounce.test"));

  AdvanceTimeTo(interaction_for_stateless_bounce + interaction_ttl +
                tiny_delta);
  EXPECT_EQ(db_->ClearRowsWithExpiredInteractions(), 1u);
  EXPECT_THAT(db_->GetAllSitesForTesting(), testing::IsEmpty());
}

TEST_P(DIPSDatabaseInteractionTest, ReadWithExpiredInteractions) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(dips::kFeature,
                                              {{"interaction_ttl", "10s"}});

  EXPECT_TRUE(db_->Read("storage-only.test").has_value());
  EXPECT_TRUE(db_->Read("stateful-bounce.test").has_value());
  EXPECT_TRUE(db_->Read("stateless-bounce.test").has_value());

  // Time travel to a point by which all interactions should've expired.
  AdvanceTimeTo(Time::FromDoubleT(100));
  EXPECT_EQ(db_->Read("storage-only.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("stateful-bounce.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("stateless-bounce.test"), absl::nullopt);
}

INSTANTIATE_TEST_SUITE_P(All, DIPSDatabaseInteractionTest, ::testing::Bool());

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
    // Use inf ttl to prevent interactions from expiring unintentionally.
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});
    DCHECK(db_);
    // Add entries to the database filling in the columns we want to test.
    // These test entries correspond with the following cases:
    // - a site accesses the browser's storage only
    // - a site redirects the user while accessing storage
    // - a site redirects the user (without regard to storage access)
    //  All of these entries include a user interaction.
    db_->Write("storage-only.test", storage_times, interaction_times,
               /*stateful_bounce_times=*/{}, /*bounce_times=*/{});
    db_->Write("stateful-bounce.test", storage_times, interaction_times,
               stateful_bounce_times,
               /*bounce_times=*/stateful_bounce_times);
    db_->Write("stateless-bounce.test",
               /*storage_times=*/{}, interaction_times,
               /*stateful_bounce_times=*/{}, bounce_times);
  }

 protected:
  // Rewrites the entries that were wrote in SetUp() to not have interactions.
  void ClearAllInteractions() {
    db_->Write("storage-only.test", storage_times,
               /*interaction_times=*/{},
               /*stateful_bounce_times=*/{}, /*bounce_times=*/{});
    db_->Write("stateful-bounce.test", storage_times,
               /*interaction_times=*/{}, stateful_bounce_times,
               /*bounce_times=*/stateful_bounce_times);
    db_->Write("stateless-bounce.test",
               /*storage_times=*/{}, /*interaction_times=*/{},
               /*stateful_bounce_times=*/{}, bounce_times);
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
  TimestampRange bounce_times = {stateless_bounce, stateless_bounce};
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
  EXPECT_THAT(
      db_->GetSitesThatBounced(earliest_range_start, after_interaction),
      testing::ElementsAre("stateful-bounce.test", "stateless-bounce.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBounced_RangeStartTest) {
  EXPECT_THAT(
      db_->GetSitesThatBounced(before_storage, after_interaction),
      testing::ElementsAre("stateful-bounce.test", "stateless-bounce.test"));
  // When the range begins after the stateful bounce happened,
  // "stateless-bounce.test" is returned since it bounces later.
  EXPECT_THAT(
      db_->GetSitesThatBounced(after_stateful_bounce, after_interaction),
      testing::ElementsAre("stateless-bounce.test"));
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
              testing::ElementsAre("stateful-bounce.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatBouncedWithState_RangeStartTest) {
  EXPECT_THAT(
      db_->GetSitesThatBouncedWithState(before_storage, after_interaction),
      testing::ElementsAre("stateful-bounce.test"));
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
      testing::ElementsAre("stateful-bounce.test", "storage-only.test"));
}

TEST_P(DIPSDatabaseQueryTest, GetSitesThatUsedStorage_RangeStartTest) {
  // When the range begins at 0, both sites that used storage are returned since
  // they did so after t=0.
  EXPECT_THAT(
      db_->GetSitesThatUsedStorage(before_storage, after_interaction),
      testing::ElementsAre("stateful-bounce.test", "storage-only.test"));
  // When the range begins after "storage-only.test" used storage, only
  // "stateful_bounce.test" is returned since it uses storage later.
  EXPECT_THAT(db_->GetSitesThatUsedStorage(after_storage, after_interaction),
              testing::ElementsAre("stateful-bounce.test"));
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
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});

    DCHECK(db_);
    db_->SetMaxEntriesForTesting(200);
    db_->SetPurgeEntriesForTesting(20);

    recent_interaction = Now();
    old_interaction = Now() - base::Days(180);

    recent_interaction_times = {recent_interaction, recent_interaction};
    old_interaction_times = {old_interaction, old_interaction};
  }

  void BloatBouncesForGC(int num_recent_entries, int num_old_entries) {
    DCHECK(db_);

    for (int i = 0; i < num_recent_entries; i++) {
      db_->Write(
          base::StrCat({"recent_interaction.test", base::NumberToString(i)}),
          storage_times, recent_interaction_times, stateful_bounce_times,
          bounce_times);
    }

    for (int i = 0; i < num_old_entries; i++) {
      db_->Write(
          base::StrCat({"old_interaction.test", base::NumberToString(i)}),
          storage_times, old_interaction_times, stateful_bounce_times,
          bounce_times);
    }
  }

 protected:
  base::Time recent_interaction;
  base::Time old_interaction;
  base::Time storage = Time::FromDoubleT(2);
  base::Time stateful_bounce = Time::FromDoubleT(3);
  base::Time stateless_bounce = Time::FromDoubleT(4);

  TimestampRange recent_interaction_times;
  TimestampRange old_interaction_times;
  TimestampRange storage_times = {storage, storage};
  TimestampRange stateful_bounce_times = {stateful_bounce, stateful_bounce};
  TimestampRange bounce_times = {stateful_bounce, stateless_bounce};
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

// The entries with the oldest interaction and storage times should be deleted
// first.
TEST_P(DIPSDatabaseGarbageCollectionTest, OldestEntriesRemoved) {
  db_->Write("old_interaction.test", {},
             /*interaction_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             {}, {});
  db_->Write("old_storage_old_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             /*interaction_times=*/{Time::FromDoubleT(2), Time::FromDoubleT(2)},
             {}, {});
  db_->Write("old_storage.test",
             /*storage_times=*/{Time::FromDoubleT(3), Time::FromDoubleT(3)}, {},
             {}, {});
  db_->Write("old_storage_new_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(1), Time::FromDoubleT(1)},
             /*interaction_times=*/{Time::FromDoubleT(4), Time::FromDoubleT(4)},
             {}, {});
  db_->Write("new_storage_old_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(5), Time::FromDoubleT(5)},
             /*interaction_times=*/{Time::FromDoubleT(2), Time::FromDoubleT(2)},
             {}, {});
  db_->Write("new_storage_new_interaction.test",
             /*storage_times=*/{Time::FromDoubleT(6), Time::FromDoubleT(6)},
             /*interaction_times=*/{Time::FromDoubleT(7), Time::FromDoubleT(7)},
             {}, {});

  EXPECT_EQ(db_->GarbageCollectOldest(3), static_cast<size_t>(3));
  EXPECT_EQ(db_->GetEntryCount(), static_cast<size_t>(3));

  EXPECT_THAT(db_->GetAllSitesForTesting(),
              testing::ElementsAre("new_storage_new_interaction.test",
                                   "new_storage_old_interaction.test",
                                   "old_storage_new_interaction.test"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSDatabaseGarbageCollectionTest,
                         ::testing::Bool());

// A test class that verifies DIPSDatabase database health metrics collection
// behavior. Created on-disk so opening a corrupt database file can be tested.
class DIPSDatabaseHistogramTest : public DIPSDatabaseTest {
 public:
  DIPSDatabaseHistogramTest() : DIPSDatabaseTest(false) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});
  }

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
  db_->Write("url1.test", {},
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
  db_->Write("url1.test", {},
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

class DIPSDatabaseMigrationTest : public testing::Test {
 public:
  DIPSDatabaseMigrationTest() {
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});
  }

 protected:
  base::test::ScopedFeatureList features_;

 protected:
  void MigrateDatabase() { TestDatabase db(db_path_); }

  int GetDatabaseVersion(sql::Database* db) {
    sql::Statement kGetVersionSql(
        db->GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
    if (!kGetVersionSql.Step()) {
      return 0;
    }
    return kGetVersionSql.ColumnInt(0);
  }

  std::vector<std::string> GetFirstAndLastColumnForSite(sql::Database* db,
                                                        const char* column,
                                                        const char* site) {
    std::string both_times = sql::test::ExecuteWithResults(
        db,
        base::StringPrintf("SELECT first_%s_time,last_%s_time FROM bounces "
                           "WHERE site='%s'",
                           column, column, site)
            .c_str(),
        "|", ",");

    return base::SplitString(both_times, "|", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_ALL);
  }

  std::vector<std::string> GetStorageTimes(sql::Database* db,
                                           const char* site) {
    return GetFirstAndLastColumnForSite(db, "site_storage", site);
  }

  std::vector<std::string> GetInteractionTimes(sql::Database* db,
                                               const char* site) {
    return GetFirstAndLastColumnForSite(db, "user_interaction", site);
  }

  std::vector<std::string> GetStatefulBounceTimes(sql::Database* db,
                                                  const char* site) {
    return GetFirstAndLastColumnForSite(db, "stateful_bounce", site);
  }

  // Note: Only works if db is using the v1 schema.
  std::vector<std::string> GetStatelessBounceTimes(sql::Database* db,
                                                   const char* site) {
    DCHECK(GetDatabaseVersion(db) == 1);
    return GetFirstAndLastColumnForSite(db, "stateless_bounce", site);
  }

  // Note: Only works if db is using the v2 schema.
  std::vector<std::string> GetBounceTimes(sql::Database* db, const char* site) {
    DCHECK(GetDatabaseVersion(db) == 2);
    return GetFirstAndLastColumnForSite(db, "bounce", site);
  }

  base::FilePath db_path() { return db_path_; }

 private:
  std::unique_ptr<TestDatabase> db_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  // Test setup.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("DIPS.db");
  }

  void TearDown() override {
    db_.reset();
    ASSERT_TRUE(temp_dir_.Delete());
  }
};

TEST_F(DIPSDatabaseMigrationTest, MigrateEmptyToCurrentVersion) {
  { DIPSDatabase db(db_path()); }

  // Validate aspects of current schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber);
    EXPECT_TRUE(db.DoesTableExist("bounces"));

    // The "stateless_bounce" columns should be removed, and replaced by just
    // "bounce" columns.
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_bounce_time"));
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV1ToCurrentVersion) {
  base::FilePath root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root));
  base::FilePath path_to_v1 = root.AppendASCII("chrome")
                                  .AppendASCII("test")
                                  .AppendASCII("data")
                                  .AppendASCII("dips")
                                  .AppendASCII("v1.sql");
  EXPECT_TRUE(base::PathExists(path_to_v1));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), path_to_v1));

  // Verify preconditions of the v1 database.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), 1);

    // These values are all set in v1.sql.
    EXPECT_EQ(sql::test::ExecuteWithResults(
                  &db, "SELECT * FROM bounces ORDER BY site", "|", "\n"),
              "both-bounce-kinds.test|0|0|4|4|1|4|2|6\n"
              "stateful-bounce.test|0|0|4|4|1|1|0|0\n"
              "stateless-bounce.test|0|0|4|4|0|0|1|1\n"
              "storage.test|1|1|4|4|0|0|0|0");

    // Note: that the stateful bounce happens earlier than the stateless bounce
    // this should be reflected in the first/last bounce times for this in v2.
    EXPECT_THAT(GetStatefulBounceTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    EXPECT_THAT(GetStatelessBounceTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("2", "6"));
    EXPECT_THAT(GetInteractionTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetStatefulBounceTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetStatelessBounceTimes(&db, "stateless-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "stateless-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetStorageTimes(&db, "storage.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "storage.test"),
                testing::ElementsAre("4", "4"));
  }

  MigrateDatabase();

  // Validate aspects of the database after migrating to the current version.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(GetDatabaseVersion(&db), 2);
    ASSERT_TRUE(db.DoesTableExist("bounces"));

    // The "stateless_bounce" columns should be removed, and replaced by
    // just "bounce" columns.
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_bounce_time"));
    // Verify that data is preserved across the migration.
    // Notably - all zeros are transformed to NULL and the final two
    // columns represent the first & last bounce times now.
    EXPECT_EQ(sql::test::ExecuteWithResults(
                  &db, "SELECT * FROM bounces ORDER BY site", "|", "\n"),
              "both-bounce-kinds.test|||4|4|1|4|1|6\n"
              "stateful-bounce.test|||4|4|1|1|1|1\n"
              "stateless-bounce.test|||4|4|||1|1\n"
              "storage.test|1|1|4|4||||");

    EXPECT_THAT(GetStatefulBounceTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    // The new bounce column should be populated correctly.
    EXPECT_THAT(GetBounceTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("1", "6"));
    EXPECT_THAT(GetInteractionTimes(&db, "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetStatefulBounceTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    // The new bounce column should be populated correctly.
    EXPECT_THAT(GetBounceTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetBounceTimes(&db, "stateless-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetStorageTimes(&db, "storage.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetInteractionTimes(&db, "storage.test"),
                testing::ElementsAre("4", "4"));
  }
}
