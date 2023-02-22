// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
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
  void LogDatabaseMetricsForTesting() { LogDatabaseMetrics(); }
};

enum ColumnType { kSiteStorage, kUserInteraction, kStatefulBounce, kBounce };
}  // namespace
class DIPSDatabaseTest : public testing::Test {
 public:
  explicit DIPSDatabaseTest(bool in_memory) : in_memory_(in_memory) {}

  // Small delta used to test before/after timestamps made with FromDoubleT.
  base::TimeDelta tiny_delta = base::Milliseconds(1);

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

class DIPSDatabaseErrorHistogramsTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseErrorHistogramsTest() : DIPSDatabaseTest(GetParam()) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    // Use inf ttl to prevent interactions from expiring unintentionally.
    features_.InitAndEnableFeatureWithParameters(dips::kFeature,
                                                 {{"interaction_ttl", "inf"}});
  }
};

TEST_P(DIPSDatabaseErrorHistogramsTest,
       StatefulBounceTimesNotWithinBounceTimes) {
  base::HistogramTester histograms;
  // `stateful_bounce` start is outside of `bounce_times`.
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      "INSERT INTO "
      "bounces(site,first_stateful_bounce_time,last_stateful_bounce_time,"
      "first_bounce_time,last_bounce_time) VALUES ('site.test',1,3,2,5)"));
  db_->Read("site.test");
  histograms.ExpectUniqueSample(
      "Privacy.DIPS.DIPSErrorCodes",
      DIPSErrorCode::kRead_BounceTimesIsntSupersetOfStatefulBounces, 1);
  // `stateful_bounce` end is outside of `bounce_times`.
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      "INSERT OR REPLACE INTO "
      "bounces(site,first_stateful_bounce_time,last_stateful_bounce_time,"
      "first_bounce_time,last_bounce_time) VALUES ('site.test',2,5,2,3)"));
  db_->Read("site.test");
  histograms.ExpectUniqueSample(
      "Privacy.DIPS.DIPSErrorCodes",
      DIPSErrorCode::kRead_BounceTimesIsntSupersetOfStatefulBounces, 2);

  // stateful_bounce is set but `bounce_times` is NULL.
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      "INSERT OR REPLACE INTO "
      "bounces(site,first_stateful_bounce_time,last_stateful_bounce_time,"
      "first_bounce_time,last_bounce_time) VALUES "
      "('site.test',2,3,NULL,NULL)"));
  db_->Read("site.test");
  histograms.ExpectUniqueSample(
      "Privacy.DIPS.DIPSErrorCodes",
      DIPSErrorCode::kRead_BounceTimesIsntSupersetOfStatefulBounces, 3);
}

// Verifies the histograms logged for the success case.
TEST_P(DIPSDatabaseErrorHistogramsTest,
       StatefulBounceTimesIsWithinBounceTimes) {
  base::HistogramTester histograms;
  // Both `stateful_bounce_time` fall within the `bounce_time` range.
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      "INSERT INTO "
      "bounces(site,first_stateful_bounce_time,last_stateful_bounce_time,"
      "first_bounce_time,last_bounce_time) VALUES ('site.test',2,4,1,5)"));
  db_->Read("site.test");
  histograms.ExpectBucketCount(
      "Privacy.DIPS.DIPSErrorCodes",
      DIPSErrorCode::kRead_BounceTimesIsntSupersetOfStatefulBounces, 0);
  histograms.ExpectBucketCount("Privacy.DIPS.DIPSErrorCodes",
                               DIPSErrorCode::kRead_None, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSDatabaseErrorHistogramsTest,
                         ::testing::Bool());

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

  std::pair<std::string, std::string> GetVariableColumnNames() {
    switch (column_) {
      case ColumnType::kSiteStorage:
        return {"first_site_storage_time", "last_site_storage_time"};
      case ColumnType::kUserInteraction:
        return {"first_user_interaction_time", "last_user_interaction_time"};
      case ColumnType::kStatefulBounce:
        return {"first_stateful_bounce_time", "last_stateful_bounce_time"};
      case ColumnType::kBounce:
        return {"first_bounce_time", "last_bounce_time"};
    }
  }

 private:
  ColumnType column_;
};

// Test adding entries in the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, AddBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce_1({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));
  // Verify that site is in `bounces` using Read().
  EXPECT_TRUE(db_->Read(site).has_value());
}

// Test updating entries in the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, UpdateBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce_1({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));

  // Verify that site's entry in `bounces` is now at t = 1
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_1);

  // Update site's entry with a bounce at t = 2
  TimestampRange bounce_2({Time::FromDoubleT(2), Time::FromDoubleT(3)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_2));

  // Verify that site's entry in `bounces` is now at t = 2
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_2);
}

// Test deleting an entry from the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, DeleteBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
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

  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
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

TEST_P(DIPSDatabaseAllColumnTest, ErrorHistograms_OpenEndedRange_NullStart) {
  base::HistogramTester histograms;

  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      base::StringPrintf(
          "INSERT INTO bounces(site,%s,%s) VALUES ('site.test',NULL,0)",
          GetVariableColumnNames().first.c_str(),
          GetVariableColumnNames().second.c_str())
          .c_str()));
  db_->Read("site.test");
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_OpenEndedRange_NullStart,
                                1);
}

TEST_P(DIPSDatabaseAllColumnTest, ErrorHistograms_OpenEndedRange_NullEnd) {
  base::HistogramTester histograms;
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      base::StringPrintf(
          "INSERT INTO bounces(site,%s,%s) VALUES ('site.test',0,NULL)",
          GetVariableColumnNames().first.c_str(),
          GetVariableColumnNames().second.c_str())
          .c_str()));
  db_->Read("site.test");
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_OpenEndedRange_NullEnd, 1);
}

// Verifies the histograms logged for the success case.
TEST_P(DIPSDatabaseAllColumnTest, ErrorHistograms_EmptyRangeExcluded) {
  base::HistogramTester histograms;
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      base::StringPrintf("INSERT INTO bounces(site,%s,%s) VALUES "
                         "('empty-site.test',NULL,NULL)",
                         GetVariableColumnNames().first.c_str(),
                         GetVariableColumnNames().second.c_str())
          .c_str()));
  db_->Read("empty-site.test");
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_None, 1);
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
               {{interaction_for_storage, interaction_for_storage}},
               /*stateful_bounce_times=*/{}, /*bounce_times=*/{});
    db_->Write(
        "stateful-bounce.test", stateful_bounce_times,
        {{interaction_for_stateful_bounce, interaction_for_stateful_bounce}},
        stateful_bounce_times,
        /*bounce_times=*/stateful_bounce_times);
    db_->Write(
        "stateless-bounce.test",
        /*storage_times=*/{},
        {{interaction_for_stateless_bounce, interaction_for_stateless_bounce}},
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

  TimestampRange storage_times = {{storage, storage}};
  TimestampRange stateful_bounce_times = {{stateful_bounce, stateful_bounce}};
  TimestampRange bounce_times = {{stateless_bounce, stateless_bounce}};
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
// DIPSDatabase to find all sites which should have their state cleared by DIPS.
class DIPSDatabaseQueryTest : public DIPSDatabaseTest,
                              public testing::WithParamInterface<
                                  std::tuple<bool, DIPSTriggeringAction>> {
 public:
  using QueryMethod = base::RepeatingCallback<std::vector<std::string>(void)>;
  DIPSDatabaseQueryTest() : DIPSDatabaseTest(std::get<0>(GetParam())) {
    // Set a constant interaction_ttl = 1 year for testing
    // This means sites protected by an interaction will stay protected until
    // the interaction expires a year later.
    int year_in_hours = base::Days(365).InHours();
    // Set a constant grace_period = 10s for testing.
    features_.InitAndEnableFeatureWithParameters(
        dips::kFeature,
        {{"interaction_ttl", base::StringPrintf("%dh", year_in_hours)},
         {"grace_period", "10s"}});
  }

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    ASSERT_EQ(dips::kGracePeriod.Get(), base::Seconds(10));
    grace_period = dips::kGracePeriod.Get();
    ASSERT_EQ(dips::kInteractionTtl.Get(), base::Days(365));
    interaction_ttl = dips::kInteractionTtl.Get();
  }

  // Returns the DIPS-triggering action we're testing.
  DIPSTriggeringAction CurrentAction() { return std::get<1>(GetParam()); }

  // Returns a callback for the respective querying method we want to test.
  QueryMethod GetQueryMethodUnderTest() {
    switch (CurrentAction()) {
      case DIPSTriggeringAction::kNone:
        return base::BindLambdaForTesting(
            [&]() { return std::vector<std::string>{}; });
      case DIPSTriggeringAction::kBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBounced(); });
      case DIPSTriggeringAction::kStorage:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatUsedStorage(); });
      case DIPSTriggeringAction::kStatefulBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBouncedWithState(); });
    }
  }

  void WriteForCurrentAction(const std::string& site,
                             TimestampRange event_times,
                             TimestampRange interactions) {
    switch (CurrentAction()) {
      case DIPSTriggeringAction::kNone:
        break;
      case DIPSTriggeringAction::kBounce:
        db_->Write(site, /*storage_times=*/{}, interactions,
                   /*stateful_bounce_times=*/{},
                   /*bounce_times=*/event_times);
        break;
      case DIPSTriggeringAction::kStorage:
        db_->Write(site, /*storage_times=*/event_times, interactions,
                   /*stateful_bounce_times=*/{}, /*bounce_times=*/{});
        break;
      case DIPSTriggeringAction::kStatefulBounce:
        db_->Write(site, /*storage_times=*/{}, interactions,
                   /*stateful_bounce_times=*/event_times,
                   /*bounce_times=*/event_times);
        break;
    }
  }

 protected:
  base::TimeDelta grace_period;
  base::TimeDelta interaction_ttl;
};

TEST_P(DIPSDatabaseQueryTest, ProtectedDuringGracePeriod) {
  // The result of running `query` shouldn't include sites which are currently
  // in their grace period after first performing a DIPS-triggering event.
  QueryMethod query = GetQueryMethodUnderTest();

  base::Time event = Time::FromDoubleT(1);
  TimestampRange event_times = {{event, event}};

  WriteForCurrentAction("site.test", event_times, /*interactions=*/{});

  // Time-travel to the start of the grace period of the triggering event.
  AdvanceTimeTo(event);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // Time-travel to within the grace period of the triggering event.
  AdvanceTimeTo(event + (grace_period / 2));
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // Time-travel to the end of the grace period of the triggering event.
  AdvanceTimeTo(event + grace_period);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // Site is no longer protected right after the grace period ends.
  AdvanceTimeTo(event + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByInteractionBeforeGracePeriod) {
  // The result of running `query` shouldn't include sites who've received
  // interactions from the user before performing a DIPS-triggering event.
  QueryMethod query = GetQueryMethodUnderTest();

  // Set up an interaction that happens before the event.
  base::Time interaction = Time::FromDoubleT(1);
  TimestampRange interaction_times = {{interaction, interaction}};
  base::Time event = Time::FromDoubleT(2);
  TimestampRange event_times = {{event, event}};

  WriteForCurrentAction("site.test", event_times, interaction_times);

  // 'site.test' shouldn't be returned when querying after the grace period
  // as the early interaction protects it from being cleared.
  AdvanceTimeTo(event + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" should still be protected up until the interaction expires.
  AdvanceTimeTo(interaction + interaction_ttl - tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // Once `interaction` expires, "site.test" restarts the DIPS-procedure and
  // `interaction` no longer protects it from DIPS clearing.
  AdvanceTimeTo(interaction + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  base::Time after_interaction_expiry = Now();
  WriteForCurrentAction(
      "site.test", {{after_interaction_expiry, after_interaction_expiry}}, {});

  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" is no longer protected by `interaction` and is cleared right
  // after its grace period ends.
  AdvanceTimeTo(after_interaction_expiry + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByInteractionDuringGracePeriod) {
  // The result of running `query` shouldn't include sites who've received
  // interactions during the grace period following a DIPS-triggering event.
  QueryMethod query = GetQueryMethodUnderTest();

  // Set up an interaction that happens during the event's grace period.
  base::Time event = Time::FromDoubleT(1);
  TimestampRange event_times = {{event, event}};
  base::Time interaction = Time::FromDoubleT(4);
  TimestampRange interaction_times = {{interaction, interaction}};
  ASSERT_TRUE(interaction < event + grace_period);

  WriteForCurrentAction("site.test", event_times, interaction_times);

  // 'site.test' shouldn't be returned as the interaction protects
  // it from being cleared.
  AdvanceTimeTo(event + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" should still be protected up until the interaction expires.
  AdvanceTimeTo(interaction + interaction_ttl - tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // Once `interaction` expires, "site.test" restarts the DIPS-procedure and
  // `interaction` no longer protects it from DIPS clearing.
  AdvanceTimeTo(interaction + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());

  base::Time after_interaction_expiry = Now();
  WriteForCurrentAction(
      "site.test", {{after_interaction_expiry, after_interaction_expiry}}, {});

  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" is no longer protected by `interaction` and is cleared right
  // after its grace period ends.
  AdvanceTimeTo(after_interaction_expiry + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

TEST_P(DIPSDatabaseQueryTest, SiteWithoutInteractionsAreUnprotected) {
  // The result of running `query` should include sites who've never received
  // interaction from the user before, or during the grace period after,
  // performing a DIPS-triggering event.
  base::RepeatingCallback<std::vector<std::string>(void)> query =
      GetQueryMethodUnderTest();

  // Set up an event with no corresponding interaction.
  base::Time event = Time::FromDoubleT(2);
  TimestampRange event_times = {{event, event}};

  WriteForCurrentAction("site.test", event_times, {});

  // 'site.test' is returned since there is no interaction protecting it.
  AdvanceTimeTo(event + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DIPSDatabaseQueryTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(DIPSTriggeringAction::kBounce,
                          DIPSTriggeringAction::kStorage,
                          DIPSTriggeringAction::kStatefulBounce)));

// A test class that verifies DIPSDatabase garbage collection behavior.
class DIPSDatabaseGarbageCollectionTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseGarbageCollectionTest() : DIPSDatabaseTest(true) {}

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    features_.InitAndEnableFeatureWithParameters(
        dips::kFeature,
        {{"interaction_ttl",
          base::StringPrintf("%dh", base::Days(90).InHours())}});

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
  TimestampRange storage_times = {{storage, storage}};
  TimestampRange stateful_bounce_times = {{stateful_bounce, stateful_bounce}};
  TimestampRange bounce_times = {{stateful_bounce, stateless_bounce}};
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

// Less than |max_entries_| entries, none of which are expired;
// no entries should be garbage collected.
TEST_P(DIPSDatabaseGarbageCollectionTest, PreservesUnderMax) {
  BloatBouncesForGC(
      /*num_recent_entries=*/(db_->GetMaxEntries() - db_->GetPurgeEntries()) /
          2,
      /*num_old_entries=*/0);

  EXPECT_EQ(db_->GarbageCollect(), static_cast<size_t>(0));

  EXPECT_EQ(db_->GetEntryCount(),
            (db_->GetMaxEntries() - db_->GetPurgeEntries()) / 2);
}

// Exactly |max_entries_| entries, none of which are expired;
// no entries should be garbage collected.
TEST_P(DIPSDatabaseGarbageCollectionTest, PreservesMax) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries(),
                    /*num_old_entries=*/0);

  EXPECT_EQ(db_->GarbageCollect(), static_cast<size_t>(0));
  EXPECT_EQ(db_->GetEntryCount(), db_->GetMaxEntries());
}

// More than |max_entries_| entries with recent user interaction and a few with
// expired user interaction; only entries with expired user interaction should
// be garbage collected by pure expiration.
TEST_P(DIPSDatabaseGarbageCollectionTest, ExpirationPreservesRecent) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() * 2,
                    /*num_old_entries=*/db_->GetMaxEntries() / 2);

  EXPECT_EQ(db_->ClearRowsWithExpiredInteractions(), db_->GetMaxEntries() / 2);

  EXPECT_EQ(db_->GetEntryCount(), db_->GetMaxEntries() * 2);
}

// The entries with the oldest interaction and storage times should be deleted
// first.
TEST_P(DIPSDatabaseGarbageCollectionTest, OldestEntriesRemoved) {
  db_->Write(
      "old_interaction.test", {},
      /*interaction_times=*/{{Time::FromDoubleT(1), Time::FromDoubleT(1)}}, {},
      {});
  db_->Write(
      "old_storage_old_interaction.test",
      /*storage_times=*/{{Time::FromDoubleT(1), Time::FromDoubleT(1)}},
      /*interaction_times=*/{{Time::FromDoubleT(2), Time::FromDoubleT(2)}}, {},
      {});
  db_->Write("old_storage.test",
             /*storage_times=*/{{Time::FromDoubleT(3), Time::FromDoubleT(3)}},
             {}, {}, {});
  db_->Write(
      "old_storage_new_interaction.test",
      /*storage_times=*/{{Time::FromDoubleT(1), Time::FromDoubleT(1)}},
      /*interaction_times=*/{{Time::FromDoubleT(4), Time::FromDoubleT(4)}}, {},
      {});
  db_->Write(
      "new_storage_old_interaction.test",
      /*storage_times=*/{{Time::FromDoubleT(5), Time::FromDoubleT(5)}},
      /*interaction_times=*/{{Time::FromDoubleT(2), Time::FromDoubleT(2)}}, {},
      {});
  db_->Write(
      "new_storage_new_interaction.test",
      /*storage_times=*/{{Time::FromDoubleT(6), Time::FromDoubleT(6)}},
      /*interaction_times=*/{{Time::FromDoubleT(7), Time::FromDoubleT(7)}}, {},
      {});

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
  db_->Write(
      "url1.test", {},
      /*interaction_times=*/{{Time::FromDoubleT(1), Time::FromDoubleT(1)}}, {},
      {});
  db_->LogDatabaseMetricsForTesting();

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
  db_->Write(
      "url1.test", {},
      /*interaction_times=*/{{Time::FromDoubleT(1), Time::FromDoubleT(1)}}, {},
      {});
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
