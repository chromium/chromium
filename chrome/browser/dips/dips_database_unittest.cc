// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_database.h"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::Time;

class DIPSDatabase;

namespace {

const int kCurrentVersionNumber = 4;
const int kCompatibleVersionNumber = 4;

class TestDatabase : public DIPSDatabase {
 public:
  explicit TestDatabase(const absl::optional<base::FilePath>& db_path)
      : DIPSDatabase(db_path) {}
  void LogDatabaseMetricsForTesting() { LogDatabaseMetrics(); }
};

enum ColumnType {
  kSiteStorage,
  kUserInteraction,
  kStatefulBounce,
  kBounce,
  kWebAuthnAssertion
};
}  // namespace

class DIPSDatabaseTest : public testing::Test {
 public:
  explicit DIPSDatabaseTest(bool in_memory) : in_memory_(in_memory) {}

  // Small delta used to test before/after timestamps made with FromDoubleT.
  base::TimeDelta tiny_delta = base::Milliseconds(1);

  TimestampRange ToRange(base::Time& time) { return {{time, time}}; }

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
    if (!in_memory_) {
      ASSERT_TRUE(temp_dir_.Delete());
    }
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
    // Use inf ttl to prevent interactions (including web authn assertions) from
    // expiring unintentionally.
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

TEST_P(DIPSDatabaseErrorHistogramsTest, kRead_EmptySite_InDb) {
  base::HistogramTester histograms;
  // Manually write an entry with an empty string `site`, then try to read it.
  ASSERT_TRUE(db_->ExecuteSqlForTesting(
      "INSERT INTO "
      "bounces(site,first_stateful_bounce_time,last_stateful_bounce_time,"
      "first_bounce_time,last_bounce_time) VALUES ('',2,4,1,5)"));
  EXPECT_EQ(db_->GetEntryCount(DIPSDatabaseTable::kBounces), 1u);
  EXPECT_EQ(db_->Read(""), absl::nullopt);
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_EmptySite_InDb, 1);
  // Verify the entry was deleted during the read attempt.
  EXPECT_EQ(db_->GetEntryCount(DIPSDatabaseTable::kBounces), 0u);
}

TEST_P(DIPSDatabaseErrorHistogramsTest, Read_EmptySite_NotInDb) {
  base::HistogramTester histograms;
  EXPECT_EQ(db_->Read(""), absl::nullopt);
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_EmptySite_NotInDb, 1);
}

TEST_P(DIPSDatabaseErrorHistogramsTest, Write_EmptySite) {
  base::HistogramTester histograms;
  // Attempt to add a bounce for an empty site.
  const std::string empty_site = GetSiteForDIPS(GURL(""));
  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_FALSE(db_->Write(empty_site, TimestampRange(), TimestampRange(),
                          TimestampRange(), bounce, TimestampRange()));
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kWrite_EmptySite, 1);
}

// Verifies the histograms logged for the success case (i.e., writing an entry
// with a non-empty site).
TEST_P(DIPSDatabaseErrorHistogramsTest, Write_None) {
  base::HistogramTester histograms;
  // Add a bounce for a non-empty site.
  const std::string site = GetSiteForDIPS(GURL("https://example.test"));
  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_TRUE(db_->Write(site, TimestampRange(), TimestampRange(),
                         TimestampRange(), bounce, TimestampRange()));
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kWrite_None, 1);
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
    // Use inf ttl to prevent interactions (including webauthn assertions) from
    // expiring unintentionally.
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
                      IsBounce(column_) ? times : TimestampRange(),
                      column_ == kWebAuthnAssertion ? times : TimestampRange());
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
      case ColumnType::kWebAuthnAssertion:
        return value->web_authn_assertion_times;
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
      case ColumnType::kWebAuthnAssertion:
        return {"first_web_authn_assertion_time",
                "last_web_authn_assertion_time"};
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

  // Delete site's entry in bounces.
  EXPECT_TRUE(db_->RemoveRow(DIPSDatabaseTable::kBounces, site));

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

  // Delete site's entry in bounces.
  EXPECT_TRUE(db_->RemoveRows(DIPSDatabaseTable::kBounces, {site1, site2}));

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

// Verifies actions on the `popups` table of the DIPS database.
class DIPSDatabasePopupsTest : public DIPSDatabaseTest,
                               public testing::WithParamInterface<bool> {
 public:
  DIPSDatabasePopupsTest() : DIPSDatabaseTest(GetParam()) {}
};

// Test adding entries in the `popups` table of the DIPSDatabase.
TEST_P(DIPSDatabasePopupsTest, AddPopup) {
  const std::string opener_site =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  uint64_t access_id = 123;
  base::Time popup_time = Time::FromDoubleT(1);

  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time));

  auto popups_state_value = db_->ReadPopup(opener_site, popup_site);
  ASSERT_TRUE(popups_state_value.has_value());
  EXPECT_EQ(popups_state_value.value().access_id, access_id);
  EXPECT_EQ(popups_state_value.value().last_popup_time, popup_time);
}

// Test updating entries in the `popups` table of the DIPSDatabase.
TEST_P(DIPSDatabasePopupsTest, UpdatePopup) {
  const std::string opener_site =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  uint64_t first_access_id = 123;
  uint64_t second_access_id = 456;
  base::Time first_popup_time = Time::FromDoubleT(1);
  base::Time second_popup_time = Time::FromDoubleT(2);

  // Write the initial entry and verify it was added to the db.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, first_access_id,
                              first_popup_time));
  EXPECT_EQ(db_->ReadPopup(opener_site, popup_site)
                .value_or(PopupsStateValue())
                .last_popup_time,
            first_popup_time);

  // Update the entry with a new popup time of t = 2.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, second_access_id,
                              second_popup_time));

  // Verify the new entry.
  auto popups_state_value = db_->ReadPopup(opener_site, popup_site);
  ASSERT_TRUE(popups_state_value.has_value());
  EXPECT_EQ(popups_state_value.value().access_id, second_access_id);
  EXPECT_EQ(popups_state_value.value().last_popup_time, second_popup_time);
}

// Test deleting an entry from the `popups` table of the DIPSDatabase. An entry
// should be deleted if the input site matches either opener_site or
// popup_site.
TEST_P(DIPSDatabasePopupsTest, DeletePopup) {
  const std::string opener_site =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  uint64_t access_id = 123;
  base::Time popup_time = Time::FromDoubleT(1);

  // Write the popup to db, and verify.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time));
  EXPECT_TRUE(db_->ReadPopup(opener_site, popup_site).has_value());

  // Delete the entry in db by opener_site, and verify.
  EXPECT_TRUE(db_->RemoveRow(DIPSDatabaseTable::kPopups, opener_site));
  EXPECT_FALSE(db_->ReadPopup(opener_site, popup_site).has_value());

  // Write the popup to db, and verify.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time));
  EXPECT_TRUE(db_->ReadPopup(opener_site, popup_site).has_value());

  // Delete the entry in db by popup_site, and verify.
  EXPECT_TRUE(db_->RemoveRow(DIPSDatabaseTable::kPopups, popup_site));
  EXPECT_FALSE(db_->ReadPopup(opener_site, popup_site).has_value());
}

// Test deleting many entries from the `popups` table of the DIPSDatabase.
TEST_P(DIPSDatabasePopupsTest, DeleteSeveralPopups) {
  // Add popups to db.
  const std::string opener_site_1 =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string opener_site_2 =
      GetSiteForDIPS(GURL("http://www.picasa.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  EXPECT_TRUE(db_->WritePopup(opener_site_1, popup_site,
                              /*access_id=*/123, Time::FromDoubleT(1)));
  EXPECT_TRUE(db_->WritePopup(opener_site_2, popup_site,
                              /*access_id=*/456, Time::FromDoubleT(2)));

  // Verify that both sites are in the `popups` table.
  EXPECT_TRUE(db_->ReadPopup(opener_site_1, popup_site).has_value());
  EXPECT_TRUE(db_->ReadPopup(opener_site_2, popup_site).has_value());

  // Delete site's entry in `popups`.
  EXPECT_TRUE(db_->RemoveRows(DIPSDatabaseTable::kBounces,
                              {opener_site_1, opener_site_2}));

  // Verify that both sites are deleted from the `popups` table.
  EXPECT_FALSE(db_->Read(opener_site_1).has_value());
  EXPECT_FALSE(db_->Read(opener_site_2).has_value());
}

INSTANTIATE_TEST_SUITE_P(All, DIPSDatabasePopupsTest, ::testing::Bool());

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
                                         ColumnType::kBounce,
                                         ColumnType::kWebAuthnAssertion)));

// A test class that verifies the behavior of the DIPSDatabase with respect to
// interactions and Web Authn Assertions (WAA).
//
// Parameterized over whether the db is in memory.
class DIPSDatabaseInteractionTest : public DIPSDatabaseTest,
                                    public testing::WithParamInterface<bool> {
 public:
  DIPSDatabaseInteractionTest() : DIPSDatabaseTest(GetParam()) {
    features_.InitAndEnableFeature(dips::kFeature);
  }

  // This test only focuses on user interaction and WAA times, that's the
  // reason why the other times like bounce times not being tested here are left
  // NULL throughout.
  void LoadDatabase() {
    DCHECK(db_);
    // Case1: last_web_authn_assertion_time == last_user_interaction_time.
    EXPECT_TRUE(db_->Write("case1.test", {}, {{dummy_time, dummy_time}}, {}, {},
                           {{dummy_time, dummy_time}}));
    // Case2: last_web_authn_assertion_time > last_user_interaction_time.
    EXPECT_TRUE(db_->Write("case2.test", {}, {{dummy_time, dummy_time}}, {}, {},
                           {{dummy_time, dummy_time + tiny_delta}}));
    // Case3: last_web_authn_assertion_time < last_user_interaction_time.
    EXPECT_TRUE(
        db_->Write("case3.test", {}, {{dummy_time, dummy_time}}, {}, {},
                   {{dummy_time - tiny_delta, dummy_time - tiny_delta}}));
    // Case4: last_web_authn_assertion_time is NULL.
    EXPECT_TRUE(
        db_->Write("case4.test", {}, {{dummy_time, dummy_time}}, {}, {}, {}));
    // Case5: last_user_interaction_time is NULL.
    EXPECT_TRUE(
        db_->Write("case5.test", {}, {}, {}, {}, {{dummy_time, dummy_time}}));
    // Case6: last_web_authn_assertion_time and last_user_interaction_time are
    // NULL.
    EXPECT_TRUE(db_->Write("case6.test", {}, {}, {}, {}, {}));
  }

 protected:
  base::Time dummy_time = Time::FromDoubleT(100);
};

TEST_P(DIPSDatabaseInteractionTest, ClearExpiredRowsFromBouncesTable) {
  LoadDatabase();

  EXPECT_THAT(
      db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::UnorderedElementsAre("case1.test", "case2.test", "case3.test",
                                    "case4.test", "case5.test", "case6.test"));

  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get());
  EXPECT_EQ(db_->ClearExpiredRows(), 0u);
  EXPECT_THAT(
      db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::UnorderedElementsAre("case1.test", "case2.test", "case3.test",
                                    "case4.test", "case5.test", "case6.test"));

  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get() + tiny_delta);
  EXPECT_EQ(db_->ClearExpiredRows(), 4u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
              testing::UnorderedElementsAre("case2.test", "case6.test"));

  // Time travel to a point by which all interactions and WAAs should've
  // expired.
  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get() + tiny_delta * 2);
  EXPECT_EQ(db_->ClearExpiredRows(), 1u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
              testing::ElementsAre("case6.test"));
}

TEST_P(DIPSDatabaseInteractionTest, ReadWithExpiredRows) {
  LoadDatabase();

  EXPECT_TRUE(db_->Read("case1.test").has_value());
  EXPECT_TRUE(db_->Read("case2.test").has_value());
  EXPECT_TRUE(db_->Read("case3.test").has_value());
  EXPECT_TRUE(db_->Read("case4.test").has_value());
  EXPECT_TRUE(db_->Read("case5.test").has_value());
  EXPECT_TRUE(db_->Read("case6.test").has_value());

  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get());
  EXPECT_TRUE(db_->Read("case1.test").has_value());
  EXPECT_TRUE(db_->Read("case2.test").has_value());
  EXPECT_TRUE(db_->Read("case3.test").has_value());
  EXPECT_TRUE(db_->Read("case4.test").has_value());
  EXPECT_TRUE(db_->Read("case5.test").has_value());
  EXPECT_TRUE(db_->Read("case6.test").has_value());

  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get() + tiny_delta);
  EXPECT_EQ(db_->Read("case1.test"), absl::nullopt);
  EXPECT_TRUE(db_->Read("case2.test").has_value());
  EXPECT_EQ(db_->Read("case3.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case4.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case5.test"), absl::nullopt);
  EXPECT_TRUE(db_->Read("case6.test").has_value());

  // Time travel to a point by which all interactions and WAAs should've
  // expired.
  AdvanceTimeTo(dummy_time + dips::kInteractionTtl.Get() + tiny_delta * 2);
  EXPECT_EQ(db_->Read("case1.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case2.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case3.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case4.test"), absl::nullopt);
  EXPECT_EQ(db_->Read("case5.test"), absl::nullopt);
  EXPECT_TRUE(db_->Read("case6.test").has_value());
}

TEST_P(DIPSDatabaseInteractionTest, ClearExpiredRowsFromPopupsTable) {
  // Add popups to db.
  const std::string opener_site_1 =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string opener_site_2 =
      GetSiteForDIPS(GURL("http://www.picasa.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  const base::Time first_popup_time = Time::FromDoubleT(1);
  const base::Time second_popup_time = Time::FromDoubleT(2);

  EXPECT_TRUE(db_->WritePopup(opener_site_1, popup_site,
                              /*access_id=*/123, first_popup_time));
  EXPECT_TRUE(db_->WritePopup(opener_site_2, popup_site,
                              /*access_id=*/456, second_popup_time));

  // Advance to just before the first popup expires.
  AdvanceTimeTo(first_popup_time + DIPSDatabase::kPopupTtl - tiny_delta);

  // Verify that both sites are present.
  EXPECT_EQ(db_->ClearExpiredRows(), 0u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kPopups),
              testing::UnorderedElementsAre("youtube.com", "doubleclick.net",
                                            "picasa.com", "doubleclick.net"));

  // Advance to after the first popup expires.
  AdvanceTimeTo(first_popup_time + DIPSDatabase::kPopupTtl + tiny_delta);

  // Verify that only the first popup was removed from the db.
  EXPECT_EQ(db_->ClearExpiredRows(), 1u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kPopups),
              testing::UnorderedElementsAre("picasa.com", "doubleclick.net"));

  // Advance to after the second popup expires.
  AdvanceTimeTo(second_popup_time + DIPSDatabase::kPopupTtl + tiny_delta);

  // Verify that both popups were removed from the db.
  EXPECT_EQ(db_->ClearExpiredRows(), 1u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kPopups),
              testing::IsEmpty());
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
    // Test with the prod feature's parameter to ensure the tested scenarios are
    // also valid/respected within prod env.
    features_.InitAndEnableFeature(dips::kFeature);
  }

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    grace_period = dips::kGracePeriod.Get();
    interaction_ttl = dips::kInteractionTtl.Get();
  }

  // Returns the DIPS-triggering action we're testing.
  DIPSTriggeringAction CurrentAction() { return std::get<1>(GetParam()); }

  // Returns a callback for the respective querying method we want to test,
  // based on `dips::kTriggeringAction`. This is equivalent to that used by
  // `DIPSStorage::GetSitesToClear` when the DIPS Timer fires.
  QueryMethod GetQueryMethodUnderTest() {
    switch (CurrentAction()) {
      case DIPSTriggeringAction::kNone:
        return base::BindLambdaForTesting(
            [&]() { return std::vector<std::string>{}; });
      case DIPSTriggeringAction::kBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBounced(grace_period); });
      case DIPSTriggeringAction::kStorage:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatUsedStorage(grace_period); });
      case DIPSTriggeringAction::kStatefulBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBouncedWithState(grace_period); });
    }
  }

  void WriteForCurrentAction(const std::string& site,
                             TimestampRange event_times,
                             TimestampRange interaction_times,
                             TimestampRange waa_times) {
    switch (CurrentAction()) {
      case DIPSTriggeringAction::kNone:
        break;
      case DIPSTriggeringAction::kBounce:
        db_->Write(site, /*storage_times=*/{}, interaction_times,
                   /*stateful_bounce_times=*/{},
                   /*bounce_times=*/event_times, waa_times);
        break;
      case DIPSTriggeringAction::kStorage:
        db_->Write(site, /*storage_times=*/event_times, interaction_times,
                   /*stateful_bounce_times=*/{}, /*bounce_times=*/{},
                   waa_times);
        break;
      case DIPSTriggeringAction::kStatefulBounce:
        db_->Write(site, /*storage_times=*/{}, interaction_times,
                   /*stateful_bounce_times=*/event_times,
                   /*bounce_times=*/event_times, waa_times);
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

  WriteForCurrentAction("site.test", event_times, {}, {});

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

  base::Time interaction = Time::FromDoubleT(1);
  TimestampRange interaction_times = {{interaction, interaction}};
  base::Time event = Time::FromDoubleT(2);
  TimestampRange event_times = {{event, event}};

  WriteForCurrentAction("site.test", event_times, interaction_times, {});

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
  WriteForCurrentAction("site.test",
                        {{after_interaction_expiry, after_interaction_expiry}},
                        {}, {});

  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" is no longer protected by `interaction` and is cleared right
  // after its grace period ends.
  AdvanceTimeTo(after_interaction_expiry + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

// The results of running `query` shouldn't include `site` with existing
// (expired or unexpired) WAAs (performed by the user before a DIPS-triggering
// event occurred).
TEST_P(DIPSDatabaseQueryTest, ProtectedByWaaBeforeGracePeriod) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Set up an event that happens after the WAA.
  {
    auto waa_time = Time::FromDoubleT(100);
    base::Time event_time = waa_time + tiny_delta;
    WriteForCurrentAction(site, {{event_time, event_time}}, {},
                          {{waa_time, waa_time}});

    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site` should remain protected by the existing WAA even after the
    // `grace_period`:
    EXPECT_GE(interaction_ttl, grace_period + tiny_delta);
    AdvanceTimeTo(event_time + grace_period + tiny_delta);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site` should still be protected before WAA expiry:
    AdvanceTimeTo(waa_time + interaction_ttl);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site`'s entry should be cleared by
    // `DIPSDatabase::ClearExpiredRows()` from the DB (hence implicitly
    // protected) after WAA expiry:
    AdvanceTimeTo(waa_time + interaction_ttl + tiny_delta);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_EQ(db_->Read(site), absl::nullopt);
  }

  // Set up an event that happens after WAA expired and the old entry cleared.
  {
    base::Time event_time = Now() + interaction_ttl + tiny_delta;
    WriteForCurrentAction(site, {{event_time, event_time}}, {}, {});
    EXPECT_THAT(query.Run(), testing::IsEmpty());

    // The `site`'s new entry is no longer protected by WAAs after the
    // `grace_period` and will be acted-upon by DIPS:
    AdvanceTimeTo(event_time + grace_period + tiny_delta);
    EXPECT_THAT(query.Run(), testing::ElementsAre(site));
    EXPECT_TRUE(db_->Read(site).has_value());
  }
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

  WriteForCurrentAction("site.test", event_times, interaction_times, {});

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
  WriteForCurrentAction("site.test",
                        {{after_interaction_expiry, after_interaction_expiry}},
                        {}, {});

  EXPECT_THAT(query.Run(), testing::IsEmpty());

  // "site.test" is no longer protected by `interaction` and is cleared right
  // after its grace period ends.
  AdvanceTimeTo(after_interaction_expiry + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

// The results of running `query` shouldn't include `site` with existing
// (expired or unexpired) WAAs (performed by the user after a DIPS-triggering
// event occurred).
TEST_P(DIPSDatabaseQueryTest, ProtectedByWaaDuringGracePeriod) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Set up an event with a WAA happening before the end of the event's
  // `grace_period`.
  {
    auto event_time = Time::FromDoubleT(100);
    base::Time waa_time = event_time + grace_period;
    WriteForCurrentAction(site, {{event_time, event_time}}, {},
                          {{waa_time, waa_time}});

    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site` should remain protected by the existing WAA even after the
    // `grace_period`:
    EXPECT_GE(interaction_ttl, grace_period + tiny_delta);
    AdvanceTimeTo(event_time + grace_period + tiny_delta);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site` should still be protected before WAA expiry:
    AdvanceTimeTo(waa_time + interaction_ttl);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_TRUE(db_->Read(site).has_value());

    // The `site`'s entry should be cleared by
    // `DIPSDatabase::ClearExpiredRows()` from the DB (hence implicitly
    // protected) after WAA expiry:
    AdvanceTimeTo(waa_time + interaction_ttl + tiny_delta);
    EXPECT_THAT(query.Run(), testing::IsEmpty());
    EXPECT_EQ(db_->Read(site), absl::nullopt);
  }

  // Set up an event that happens after WAA expired and the old entry cleared.
  {
    base::Time event_time = Now() + interaction_ttl + tiny_delta;
    WriteForCurrentAction(site, {{event_time, event_time}}, {}, {});
    EXPECT_THAT(query.Run(), testing::IsEmpty());

    // The `site`'s new entry is no longer protected by WAAs after the
    // `grace_period` and will be acted-upon by DIPS.
    AdvanceTimeTo(event_time + grace_period + tiny_delta);
    EXPECT_THAT(query.Run(), testing::ElementsAre(site));
    EXPECT_TRUE(db_->Read(site).has_value());
  }
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

  WriteForCurrentAction("site.test", event_times, {}, {});

  // 'site.test' is returned since there is no interaction protecting it.
  AdvanceTimeTo(event + grace_period + tiny_delta);
  EXPECT_THAT(query.Run(), testing::ElementsAre("site.test"));
}

// This is an edge-case and the current accepted behavior is as expressed by
// this test coverage.
TEST_P(DIPSDatabaseQueryTest, ProtectedByWaaAfterGracePeriod) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Sets up an event with a WAA happening after the end of the event's
  // `grace_period` but before the subsequent DIPS-trigger:
  auto event_time = Time::FromDoubleT(100);
  auto waa_time = event_time + grace_period + tiny_delta;
  WriteForCurrentAction(site, {{event_time, event_time}}, {},
                        {{waa_time, waa_time}});

  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site` should still be protected before WAA expiry:
  EXPECT_GT(interaction_ttl, grace_period);
  AdvanceTimeTo(waa_time + interaction_ttl);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site`'s entry should be cleared by
  // `DIPSDatabase::ClearExpiredRows()` from the DB (hence implicitly protected)
  // after WAA expiry:
  AdvanceTimeTo(waa_time + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_EQ(db_->Read(site), absl::nullopt);
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByInteractionThenWaa) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Sets up an event with a interaction happening before the end of the event's
  // `grace_period` and an WAA some moments later:
  auto event_time = Time::FromDoubleT(100);
  auto interaction_time = event_time + grace_period;
  auto waa_time = interaction_time + tiny_delta;
  WriteForCurrentAction(site, {{event_time, event_time}},
                        {{interaction_time, interaction_time}},
                        {{waa_time, waa_time}});

  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site` should still be protected after interaction expiry:
  EXPECT_GT(interaction_ttl, grace_period);
  AdvanceTimeTo(interaction_time + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site`'s entry should be cleared by
  // `DIPSDatabase::ClearExpiredRows()` from the DB (hence implicitly protected)
  // after WAA expiry:
  AdvanceTimeTo(waa_time + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_EQ(db_->Read(site), absl::nullopt);
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByWaaThenInteraction) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Sets up an event with a WAA happening before the end of the event's
  // `grace_period` and an interaction some moments later:
  auto event_time = Time::FromDoubleT(100);
  auto waa_time = event_time + tiny_delta;
  auto interaction_time = waa_time + grace_period;
  WriteForCurrentAction(site, {{event_time, event_time}},
                        {{interaction_time, interaction_time}},
                        {{waa_time, waa_time}});

  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site` should still be protected after WAA expiry:
  EXPECT_GT(interaction_ttl, grace_period);
  AdvanceTimeTo(waa_time + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_TRUE(db_->Read(site).has_value());

  // The `site`'s entry should be cleared by
  // `DIPSDatabase::ClearExpiredRows()` from the DB (hence implicitly protected)
  // after interaction expiry:
  AdvanceTimeTo(interaction_time + interaction_ttl + tiny_delta);
  EXPECT_THAT(query.Run(), testing::IsEmpty());
  EXPECT_EQ(db_->Read(site), absl::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DIPSDatabaseQueryTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(DIPSTriggeringAction::kBounce,
                          DIPSTriggeringAction::kStorage,
                          DIPSTriggeringAction::kStatefulBounce)));

// A test class that verifies DIPSDatabase garbage collection behavior for both
// tables.
class DIPSDatabaseGarbageCollectionTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<DIPSDatabaseTable> {
 public:
  DIPSDatabaseGarbageCollectionTest() : DIPSDatabaseTest(true) {
    table_ = GetParam();
  }

  explicit DIPSDatabaseGarbageCollectionTest(DIPSDatabaseTable table)
      : DIPSDatabaseTest(true) {
    table_ = table;
  }

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
  }

  void AddEntry(const std::string& site,
                TimestampRange storage_times,
                TimestampRange interaction_times,
                TimestampRange waa_times) {
    if (table_ == DIPSDatabaseTable::kBounces) {
      ASSERT_TRUE(db_->Write(site, storage_times, interaction_times, {}, {},
                             waa_times));
    } else {
      ASSERT_TRUE(db_->WritePopup(site, "doubleclick.net", /*access_id=*/123,
                                  interaction_times->second));
    }
  }

  void BloatBouncesForGC(int num_recent_entries, int num_old_entries) {
    DCHECK(db_);

    for (int i = 0; i < num_recent_entries; i++) {
      AddEntry(
          base::StrCat({"recent_interaction.test", base::NumberToString(i)}),
          ToRange(storage), ToRange(recent_interaction), {});
    }

    for (int i = 0; i < num_old_entries; i++) {
      AddEntry(base::StrCat({"old_interaction.test", base::NumberToString(i)}),
               ToRange(storage), ToRange(old_interaction), {});
    }
  }

  void LoadDatabase() {
    clock_.SetNow(Time::FromDoubleT(100));
    std::vector<base::Time> times{Now(), Now() + tiny_delta,
                                  Now() + tiny_delta * 2};

    for (int i = 1; i <= 3; i++) {
      if (table_ == DIPSDatabaseTable::kBounces) {
        ASSERT_TRUE(db_->Write(
            base::StringPrintf("entry%d.test", 7 - i), ToRange(times[i % 3]),
            ToRange(times[(i + 1) % 3]), {}, {}, ToRange(times[(i + 2) % 3])));
      } else {
        ASSERT_TRUE(db_->WritePopup(base::StringPrintf("entry%d.test", 7 - i),
                                    "doubleclick.net", /*access_id=*/123,
                                    times[(i + 1) % 3]));
      }
      for (auto& time : times) {
        time += tiny_delta * 3;
      }
    }
    for (int i = 3; i <= 6; i++) {
      if (table_ == DIPSDatabaseTable::kBounces) {
        ASSERT_TRUE(db_->Write(base::StringPrintf("entry%d.test", 7 - i),
                               ToRange(times[(i + 2) % 3]),
                               ToRange(times[(i + 1) % 3]), {}, {},
                               ToRange(times[i % 3])));
      } else {
        ASSERT_TRUE(db_->WritePopup(base::StringPrintf("entry%d.test", 7 - i),
                                    "doubleclick.net", /*access_id=*/123,
                                    times[(i + 1) % 3]));
      }
      for (auto& time : times) {
        time += tiny_delta * 3;
      }
    }
    if (table_ == DIPSDatabaseTable::kBounces) {
      EXPECT_THAT(
          db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
          testing::ElementsAre("entry1.test", "entry2.test", "entry3.test",
                               "entry4.test", "entry5.test", "entry6.test"));
    } else {
      EXPECT_THAT(
          db_->GetAllSitesForTesting(DIPSDatabaseTable::kPopups),
          testing::IsSupersetOf({"entry1.test", "entry2.test", "entry3.test",
                                 "entry4.test", "entry5.test", "entry6.test"}));
    }
  }

 protected:
  DIPSDatabaseTable table_;

  base::Time recent_interaction;
  base::Time old_interaction;
  base::Time storage = Time::FromDoubleT(2);
};

// More than |max_entries_| entries with recent user interaction; garbage
// collection should purge down to |max_entries_| - |purge_entries_| entries.
TEST_P(DIPSDatabaseGarbageCollectionTest, RemovesRecentOverMax) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() * 2,
                    /*num_old_entries=*/0);

  EXPECT_EQ(db_->GarbageCollect(),
            db_->GetMaxEntries() + db_->GetPurgeEntries());

  EXPECT_EQ(db_->GetEntryCount(GetParam()),
            db_->GetMaxEntries() - db_->GetPurgeEntries());
}

TEST_P(DIPSDatabaseGarbageCollectionTest, RemovesExpired_RemovesRecent_GT_Max) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries() * 2,
                    /*num_old_entries=*/db_->GetMaxEntries());

  EXPECT_EQ(db_->GarbageCollect(),
            db_->GetMaxEntries() * 2 + db_->GetPurgeEntries());
  EXPECT_EQ(db_->GetEntryCount(GetParam()),
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

  EXPECT_EQ(db_->GetEntryCount(GetParam()),
            (db_->GetMaxEntries() - db_->GetPurgeEntries()) / 2);
}

// Exactly |max_entries_| entries, none of which are expired;
// no entries should be garbage collected.
TEST_P(DIPSDatabaseGarbageCollectionTest, PreservesMax) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries(),
                    /*num_old_entries=*/0);

  EXPECT_EQ(db_->GarbageCollect(), static_cast<size_t>(0));
  EXPECT_EQ(db_->GetEntryCount(GetParam()), db_->GetMaxEntries());
}

TEST_P(DIPSDatabaseGarbageCollectionTest,
       RemovesExpired_PreserveRecent_LE_Max) {
  BloatBouncesForGC(/*num_recent_entries=*/db_->GetMaxEntries(),
                    /*num_old_entries=*/db_->GetMaxEntries());

  EXPECT_EQ(db_->GarbageCollect(), db_->GetMaxEntries());
  EXPECT_EQ(db_->GetEntryCount(GetParam()), db_->GetMaxEntries());
}

TEST_P(DIPSDatabaseGarbageCollectionTest, GarbageCollectOldest) {
  LoadDatabase();

  EXPECT_THAT(
      db_->GetGarbageCollectOldestSitesForTesting(GetParam()),
      testing::ElementsAre("entry6.test", "entry5.test", "entry4.test",
                           "entry3.test", "entry2.test", "entry1.test"));

  EXPECT_EQ(db_->GarbageCollectOldest(GetParam(), 3), static_cast<size_t>(3));
  if (GetParam() == DIPSDatabaseTable::kBounces) {
    EXPECT_THAT(
        db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
        testing::ElementsAre("entry1.test", "entry2.test", "entry3.test"));
  } else {
    EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kPopups),
                testing::ElementsAre("entry1.test", "doubleclick.net",
                                     "entry2.test", "doubleclick.net",
                                     "entry3.test", "doubleclick.net"));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSDatabaseGarbageCollectionTest,
                         ::testing::Values(DIPSDatabaseTable::kBounces,
                                           DIPSDatabaseTable::kPopups));

// These tests only apply to the `bounces` table.
class DIPSDatabaseBounceTableGarbageCollectionTest
    : public DIPSDatabaseGarbageCollectionTest {
 public:
  DIPSDatabaseBounceTableGarbageCollectionTest()
      : DIPSDatabaseGarbageCollectionTest(DIPSDatabaseTable::kBounces) {}
};

TEST_F(DIPSDatabaseBounceTableGarbageCollectionTest,
       GarbageCollectOldest_NullStorageTimes) {
  LoadDatabase();

  for (int i = 1; i <= 6; i++) {
    auto state = db_->Read(base::StringPrintf("entry%d.test", i));
    AddEntry(base::StringPrintf("entry%d.test", i), {},
             state->user_interaction_times, state->web_authn_assertion_times);
  }
  EXPECT_THAT(
      db_->GetGarbageCollectOldestSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::ElementsAre("entry6.test", "entry5.test", "entry4.test",
                           "entry3.test", "entry2.test", "entry1.test"));
}

TEST_F(DIPSDatabaseBounceTableGarbageCollectionTest,
       GarbageCollectOldest_NullInteractionTimes) {
  LoadDatabase();

  for (int i = 1; i <= 6; i++) {
    auto state = db_->Read(base::StringPrintf("entry%d.test", i));
    AddEntry(base::StringPrintf("entry%d.test", i), state->site_storage_times,
             {}, state->web_authn_assertion_times);
  }
  EXPECT_THAT(
      db_->GetGarbageCollectOldestSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::ElementsAre("entry6.test", "entry5.test", "entry4.test",
                           "entry3.test", "entry2.test", "entry1.test"));
}

TEST_F(DIPSDatabaseBounceTableGarbageCollectionTest,
       GarbageCollectOldest_NullWaaTimes) {
  LoadDatabase();

  for (int i = 1; i <= 6; i++) {
    auto state = db_->Read(base::StringPrintf("entry%d.test", i));
    AddEntry(base::StringPrintf("entry%d.test", i), state->site_storage_times,
             state->user_interaction_times, {});
  }
  EXPECT_THAT(
      db_->GetGarbageCollectOldestSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::ElementsAre("entry6.test", "entry5.test", "entry4.test",
                           "entry3.test", "entry2.test", "entry1.test"));
}

// Making sure having only one of storage, user interaction or WAA times
// shouldn't alter the oldest site ordering of the garbage collection. In this
// explicit case we only have user interaction times; we should expect the same
// behavior for the other times.
TEST_F(DIPSDatabaseBounceTableGarbageCollectionTest,
       GarbageCollectOldest_SingleNonNull) {
  LoadDatabase();

  for (int i = 1; i <= 6; i++) {
    auto state = db_->Read(base::StringPrintf("entry%d.test", i));
    AddEntry(base::StringPrintf("entry%d.test", i), {},
             state->user_interaction_times, {});
  }
  EXPECT_THAT(
      db_->GetGarbageCollectOldestSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::ElementsAre("entry6.test", "entry5.test", "entry4.test",
                           "entry3.test", "entry2.test", "entry1.test"));
}

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
      {}, {});
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
      {}, {});
  EXPECT_EQ(db_->GetEntryCount(DIPSDatabaseTable::kBounces),
            static_cast<size_t>(1));

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
  EXPECT_EQ(db_->GetEntryCount(DIPSDatabaseTable::kBounces),
            static_cast<size_t>(0));

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
  // Time columns root:
  const char* kStorageTimes = "site_storage";
  const char* kInteractionTimes = "user_interaction";
  const char* kStatefulBounceTimes = "stateful_bounce";
  const char* kStatelessBounceTimesV1 = "stateless_bounce";
  const char* kBounceTimesV2ToV3 = "bounce";

  void MigrateDatabase() { TestDatabase db(db_path_); }

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

  int GetDatabasePrepopulated(sql::Database* db) {
    sql::Statement kGetPrepopulatedSql(db->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='prepopulated'"));
    if (!kGetPrepopulatedSql.Step()) {
      return 0;
    }
    return kGetPrepopulatedSql.ColumnInt(0);
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

  base::FilePath db_path() { return db_path_; }

  void LoadDatabase(const char* file_name) {
    base::FilePath root;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root));
    base::FilePath file_path = root.AppendASCII("chrome")
                                   .AppendASCII("test")
                                   .AppendASCII("data")
                                   .AppendASCII("dips")
                                   .AppendASCII(file_name);
    EXPECT_TRUE(base::PathExists(file_path));
    ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));
  }

  std::string DbToString(sql::Database* db) {
    return sql::test::ExecuteWithResults(
        db, "SELECT * FROM bounces ORDER BY site", "|", "\n");
  }

 private:
  base::test::ScopedFeatureList features_;
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
    EXPECT_TRUE(db.DoesTableExist("popups"));

    // The "stateless_bounce" columns should be removed, and replaced by just
    // "bounce" columns.
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_bounce_time"));
    EXPECT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));
  }
}

TEST_F(DIPSDatabaseMigrationTest, RazeIfIncompatible_TooNew) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase("v2.sql"));

  // Manipulations on the database version number are not necessary, but
  // performed for consistency.
  //
  // Verify pre migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Matches what is in "v2.sql" file:
    const auto v2sql_version_num = 2;
    const auto v2sql_compatible_version_num = 2;
    const auto v2sql_prepopulated = 1;

    EXPECT_EQ(GetDatabaseVersion(&db), v2sql_version_num);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db),
              v2sql_compatible_version_num);
    EXPECT_EQ(GetDatabasePrepopulated(&db), v2sql_prepopulated);

    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, v2sql_version_num, v2sql_compatible_version_num));

    // Prepare simulation of raze if incompatible. by making this DB
    // incompatible.
    const int tiny_increment = 1;
    ASSERT_TRUE(
        meta_table.SetVersionNumber(kCurrentVersionNumber + tiny_increment));
    ASSERT_TRUE(meta_table.SetCompatibleVersionNumber(kCurrentVersionNumber +
                                                      tiny_increment));

    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber + tiny_increment);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db),
              kCurrentVersionNumber + tiny_increment);

    // These values are all set in v2.sql.
    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6\n"
              "stateful-bounce.test|||4|4|1|1||\n"
              "stateless-bounce.test|||4|4|||1|1\n"
              "storage.test|1|1|4|4||||");
  }

  MigrateDatabase();

  // Verify post migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // Check version.
    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), kCompatibleVersionNumber);
    EXPECT_EQ(GetDatabasePrepopulated(&db), 0);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    ASSERT_TRUE(db.DoesTableExist("popups"));

    EXPECT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    // As expected the database is razed after migration.
    EXPECT_EQ(DbToString(&db), "");
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV1ToCurrentVersion) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase("v1.sql"));

  // Verify pre migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), 1);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_bounce_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    // These values are all set in v1.sql.
    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|0|0|4|4|1|4|2|6\n"
              "stateful-bounce.test|0|0|4|4|1|1|0|0\n"
              "stateless-bounce.test|0|0|4|4|0|0|1|1\n"
              "storage.test|1|1|4|4|0|0|0|0");

    // Note: that the stateful bounce happens earlier than the stateless bounce
    // this should be reflected in the first/last bounce times for this in v2.
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatefulBounceTimes,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatelessBounceTimesV1,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("2", "6"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatefulBounceTimes,
                                             "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatelessBounceTimesV1,
                                             "stateless-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "stateless-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, kStorageTimes, "storage.test"),
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, kInteractionTimes, "storage.test"),
        testing::ElementsAre("4", "4"));
  }

  MigrateDatabase();

  // Verify post migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), kCompatibleVersionNumber);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    ASSERT_TRUE(db.DoesTableExist("popups"));

    // The `kStatelessBounceTimesV1` columns should be removed, and replaced by
    // just `kBounceTimesV2ToV3` columns:
    EXPECT_FALSE(db.DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db.DoesColumnExist("bounces", "last_stateless_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_bounce_time"));

    // Web authn assertion time columns are added:
    EXPECT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    // Verifies that data is preserved across the migration.
    // Notably:
    // - All zeros are transformed to NULL, and
    // - Four extra columns were added.
    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|1|6||\n"
              "stateful-bounce.test|||4|4|1|1|1|1||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatefulBounceTimes,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("1", "4"));
    // The new bounce column should be populated correctly.
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kBounceTimesV2ToV3,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("1", "6"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "both-bounce-kinds.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kStatefulBounceTimes,
                                             "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    // The new bounce column should be populated correctly.
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kBounceTimesV2ToV3,
                                             "stateful-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kBounceTimesV2ToV3,
                                             "stateless-bounce.test"),
                testing::ElementsAre("1", "1"));
    EXPECT_THAT(GetFirstAndLastColumnForSite(&db, kInteractionTimes,
                                             "stateful-bounce.test"),
                testing::ElementsAre("4", "4"));

    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, kStorageTimes, "storage.test"),
        testing::ElementsAre("1", "1"));
    EXPECT_THAT(
        GetFirstAndLastColumnForSite(&db, kInteractionTimes, "storage.test"),
        testing::ElementsAre("4", "4"));
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV2ToCurrentVersion) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase("v2.sql"));

  // Verify pre migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), 2);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 2);
    EXPECT_EQ(GetDatabasePrepopulated(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_FALSE(
        db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6\n"
              "stateful-bounce.test|||4|4|1|1||\n"
              "stateless-bounce.test|||4|4|||1|1\n"
              "storage.test|1|1|4|4||||");
  }

  MigrateDatabase();

  // Verify post migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), kCompatibleVersionNumber);
    EXPECT_EQ(GetDatabasePrepopulated(&db), 1);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    ASSERT_TRUE(db.DoesTableExist("popups"));

    EXPECT_TRUE(
        db.DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(db.DoesColumnExist("bounces", "last_web_authn_assertion_time"));

    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6||\n"
              "stateful-bounce.test|||4|4|1|1||||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");
  }
}

TEST_F(DIPSDatabaseMigrationTest, MigrateV3ToCurrentVersion) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase("v3.sql"));

  // Verify pre migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), 3);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 3);
    EXPECT_EQ(GetDatabasePrepopulated(&db), 1);

    EXPECT_FALSE(db.DoesTableExist("popups"));

    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6||\n"
              "stateful-bounce.test|||4|4|1|1||||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");
  }

  MigrateDatabase();

  // Verify post migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), kCurrentVersionNumber);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), kCompatibleVersionNumber);
    EXPECT_EQ(GetDatabasePrepopulated(&db), 1);

    ASSERT_TRUE(db.DoesTableExist("bounces"));
    ASSERT_TRUE(db.DoesTableExist("popups"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "opener_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "popup_site"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "access_id"));
    EXPECT_TRUE(db.DoesColumnExist("popups", "last_popup_time"));

    EXPECT_EQ(DbToString(&db),
              "both-bounce-kinds.test|||4|4|1|4|2|6||\n"
              "stateful-bounce.test|||4|4|1|1||||\n"
              "stateless-bounce.test|||4|4|||1|1||\n"
              "storage.test|1|1|4|4||||||");
  }
}
