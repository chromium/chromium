// Copyright 2022 The Chromium Authors
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
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/dips_utils.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using testing::Optional;

class DIPSDatabase;

namespace {

class TestDatabase : public DIPSDatabase {
 public:
  explicit TestDatabase(const std::optional<base::FilePath>& db_path)
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

  // Small delta used to test before/after timestamps made with
  // FromSecondsSinceUnixEpoch.
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
      db_ = std::make_unique<TestDatabase>(std::nullopt);
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
    features_.InitAndEnableFeatureWithParameters(features::kDIPS,
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
  EXPECT_EQ(db_->Read(""), std::nullopt);
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_EmptySite_InDb, 1);
  // Verify the entry was deleted during the read attempt.
  EXPECT_EQ(db_->GetEntryCount(DIPSDatabaseTable::kBounces), 0u);
}

TEST_P(DIPSDatabaseErrorHistogramsTest, Read_EmptySite_NotInDb) {
  base::HistogramTester histograms;
  EXPECT_EQ(db_->Read(""), std::nullopt);
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_EmptySite_NotInDb, 1);
}

TEST_P(DIPSDatabaseErrorHistogramsTest, Write_EmptySite) {
  base::HistogramTester histograms;
  // Attempt to add a bounce for an empty site.
  const std::string empty_site = GetSiteForDIPS(GURL(""));
  TimestampRange bounce(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
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
  TimestampRange bounce(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
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
    features_.InitAndEnableFeatureWithParameters(features::kDIPS,
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

  TimestampRange ReadValueForVariableColumn(std::optional<StateValue> value) {
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
  TimestampRange bounce_1(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));
  // Verify that site is in `bounces` using Read().
  EXPECT_TRUE(db_->Read(site).has_value());
}

// Test updating entries in the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, UpdateBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce_1(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_1));

  // Verify that site's entry in `bounces` is now at t = 1
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_1);

  // Update site's entry with a bounce at t = 2
  TimestampRange bounce_2(
      {Time::FromSecondsSinceUnixEpoch(2), Time::FromSecondsSinceUnixEpoch(3)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce_2));

  // Verify that site's entry in `bounces` is now at t = 2
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce_2);
}

// Test deleting an entry from the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, DeleteBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  TimestampRange bounce(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
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

  TimestampRange bounce(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
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

  TimestampRange bounce(
      {Time::FromSecondsSinceUnixEpoch(1), Time::FromSecondsSinceUnixEpoch(1)});
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
  base::Time popup_time = Time::FromSecondsSinceUnixEpoch(1);
  bool is_current_interaction = true;
  bool is_authentication_interaction = true;

  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time,
                              is_current_interaction,
                              is_authentication_interaction));

  auto popups_state_value = db_->ReadPopup(opener_site, popup_site);
  ASSERT_TRUE(popups_state_value.has_value());
  EXPECT_EQ(popups_state_value.value().access_id, access_id);
  EXPECT_EQ(popups_state_value.value().last_popup_time, popup_time);
  EXPECT_EQ(popups_state_value.value().is_current_interaction,
            is_current_interaction);
  EXPECT_EQ(popups_state_value.value().is_authentication_interaction,
            is_authentication_interaction);
}

// Test updating entries in the `popups` table of the DIPSDatabase.
TEST_P(DIPSDatabasePopupsTest, UpdatePopup) {
  const std::string opener_site =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  uint64_t first_access_id = 123;
  uint64_t second_access_id = 456;
  base::Time first_popup_time = Time::FromSecondsSinceUnixEpoch(1);
  base::Time second_popup_time = Time::FromSecondsSinceUnixEpoch(2);

  // Write the initial entry and verify it was added to the db.
  EXPECT_TRUE(db_->WritePopup(
      opener_site, popup_site, first_access_id, first_popup_time,
      /*is_current_interaction=*/true, /*is_authentication_interaction=*/true));
  auto popups_state_value = db_->ReadPopup(opener_site, popup_site);
  EXPECT_EQ(popups_state_value.value_or(PopupsStateValue()).last_popup_time,
            first_popup_time);
  EXPECT_EQ(popups_state_value.value().is_authentication_interaction, true);

  // Update the entry with a new popup time of t = 2, is_current_interaction =
  // false, and is_authentication_interaction = false.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, second_access_id,
                              second_popup_time,
                              /*is_current_interaction=*/false,
                              /*is_authentication_interaction=*/false));

  // Verify the new entry.
  popups_state_value = db_->ReadPopup(opener_site, popup_site);
  ASSERT_TRUE(popups_state_value.has_value());
  EXPECT_EQ(popups_state_value.value().access_id, second_access_id);
  EXPECT_EQ(popups_state_value.value().last_popup_time, second_popup_time);
  EXPECT_EQ(popups_state_value.value().is_current_interaction, false);
  EXPECT_EQ(popups_state_value.value().is_authentication_interaction, false);
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
  base::Time popup_time = Time::FromSecondsSinceUnixEpoch(1);

  // Write the popup to db, and verify.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time,
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));
  EXPECT_TRUE(db_->ReadPopup(opener_site, popup_site).has_value());

  // Delete the entry in db by opener_site, and verify.
  EXPECT_TRUE(db_->RemoveRow(DIPSDatabaseTable::kPopups, opener_site));
  EXPECT_FALSE(db_->ReadPopup(opener_site, popup_site).has_value());

  // Write the popup to db, and verify.
  EXPECT_TRUE(db_->WritePopup(opener_site, popup_site, access_id, popup_time,
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));
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
  EXPECT_TRUE(db_->WritePopup(
      opener_site_1, popup_site,
      /*access_id=*/123, Time::FromSecondsSinceUnixEpoch(1),
      /*is_current_interaction=*/true, /*is_authentication_interaction=*/true));
  EXPECT_TRUE(db_->WritePopup(opener_site_2, popup_site,
                              /*access_id=*/456,
                              Time::FromSecondsSinceUnixEpoch(2),
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));

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

// Test the `ReadRecentPopupsWithInteraction` function which retrieves a list of
// `popups` table entries with recent popup timestamps.
TEST_P(DIPSDatabasePopupsTest, ReadRecentPopupsWithInteraction) {
  base::Time now = Now();

  // Add popups to db.
  const std::string opener_site_1 =
      GetSiteForDIPS(GURL("http://www.youtube.com/"));
  const std::string opener_site_2 =
      GetSiteForDIPS(GURL("http://www.picasa.com/"));
  const std::string opener_site_3 =
      GetSiteForDIPS(GURL("http://www.google.com/"));
  const std::string popup_site =
      GetSiteForDIPS(GURL("http://www.doubleclick.net/"));
  EXPECT_TRUE(db_->WritePopup(opener_site_1, popup_site,
                              /*access_id=*/123, now - base::Seconds(10),
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));
  EXPECT_TRUE(db_->WritePopup(opener_site_2, popup_site,
                              /*access_id=*/456, now - base::Seconds(10),
                              /*is_current_interaction=*/false,
                              /*is_authentication_interaction=*/false));
  EXPECT_TRUE(db_->WritePopup(opener_site_3, popup_site,
                              /*access_id=*/789, now - base::Seconds(30),
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));

  // Verify that all three sites are in the `popups` table.
  EXPECT_TRUE(db_->ReadPopup(opener_site_1, popup_site).has_value());
  EXPECT_TRUE(db_->ReadPopup(opener_site_2, popup_site).has_value());
  EXPECT_TRUE(db_->ReadPopup(opener_site_3, popup_site).has_value());

  // Expect no popups recorded in the last 5 seconds.
  std::vector<PopupWithTime> very_recent_popups =
      db_->ReadRecentPopupsWithInteraction(base::Seconds(5));
  EXPECT_TRUE(very_recent_popups.empty());

  // Expect one popup in the last 20 seconds with a current interaction.
  std::vector<PopupWithTime> recent_popups =
      db_->ReadRecentPopupsWithInteraction(base::Seconds(20));
  ASSERT_EQ(recent_popups.size(), 1u);
  EXPECT_EQ(recent_popups.at(0).opener_site, opener_site_1);
  EXPECT_EQ(recent_popups.at(0).popup_site, popup_site);
  EXPECT_EQ(recent_popups.at(0).last_popup_time, now - base::Seconds(10));
}

INSTANTIATE_TEST_SUITE_P(All, DIPSDatabasePopupsTest, ::testing::Bool());

TEST_P(DIPSDatabaseAllColumnTest, ErrorHistograms_OpenEndedRange_NullStart) {
  base::HistogramTester histograms;

  ASSERT_TRUE(db_->ExecuteSqlForTesting(base::StringPrintf(
      "INSERT INTO bounces(site,%s,%s) VALUES ('site.test',NULL,0)",
      GetVariableColumnNames().first.c_str(),
      GetVariableColumnNames().second.c_str())));
  db_->Read("site.test");
  histograms.ExpectUniqueSample("Privacy.DIPS.DIPSErrorCodes",
                                DIPSErrorCode::kRead_OpenEndedRange_NullStart,
                                1);
}

TEST_P(DIPSDatabaseAllColumnTest, ErrorHistograms_OpenEndedRange_NullEnd) {
  base::HistogramTester histograms;
  ASSERT_TRUE(db_->ExecuteSqlForTesting(base::StringPrintf(
      "INSERT INTO bounces(site,%s,%s) VALUES ('site.test',0,NULL)",
      GetVariableColumnNames().first.c_str(),
      GetVariableColumnNames().second.c_str())));
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
                         GetVariableColumnNames().second.c_str())));
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
    features_.InitAndEnableFeature(features::kDIPS);
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
  base::Time dummy_time = Time::FromSecondsSinceUnixEpoch(100);
};

TEST_P(DIPSDatabaseInteractionTest, ClearExpiredRowsFromBouncesTable) {
  LoadDatabase();

  EXPECT_THAT(
      db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::UnorderedElementsAre("case1.test", "case2.test", "case3.test",
                                    "case4.test", "case5.test", "case6.test"));

  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get());
  EXPECT_EQ(db_->ClearExpiredRows(), 0u);
  EXPECT_THAT(
      db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
      testing::UnorderedElementsAre("case1.test", "case2.test", "case3.test",
                                    "case4.test", "case5.test", "case6.test"));

  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get() + tiny_delta);
  EXPECT_EQ(db_->ClearExpiredRows(), 4u);
  EXPECT_THAT(db_->GetAllSitesForTesting(DIPSDatabaseTable::kBounces),
              testing::UnorderedElementsAre("case2.test", "case6.test"));

  // Time travel to a point by which all interactions and WAAs should've
  // expired.
  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get() +
                tiny_delta * 2);
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

  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get());
  EXPECT_TRUE(db_->Read("case1.test").has_value());
  EXPECT_TRUE(db_->Read("case2.test").has_value());
  EXPECT_TRUE(db_->Read("case3.test").has_value());
  EXPECT_TRUE(db_->Read("case4.test").has_value());
  EXPECT_TRUE(db_->Read("case5.test").has_value());
  EXPECT_TRUE(db_->Read("case6.test").has_value());

  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get() + tiny_delta);
  EXPECT_EQ(db_->Read("case1.test"), std::nullopt);
  EXPECT_TRUE(db_->Read("case2.test").has_value());
  EXPECT_EQ(db_->Read("case3.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case4.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case5.test"), std::nullopt);
  EXPECT_TRUE(db_->Read("case6.test").has_value());

  // Time travel to a point by which all interactions and WAAs should've
  // expired.
  AdvanceTimeTo(dummy_time + features::kDIPSInteractionTtl.Get() +
                tiny_delta * 2);
  EXPECT_EQ(db_->Read("case1.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case2.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case3.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case4.test"), std::nullopt);
  EXPECT_EQ(db_->Read("case5.test"), std::nullopt);
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
  const base::Time first_popup_time = Time::FromSecondsSinceUnixEpoch(1);
  const base::Time second_popup_time = Time::FromSecondsSinceUnixEpoch(2);

  EXPECT_TRUE(db_->WritePopup(opener_site_1, popup_site,
                              /*access_id=*/123, first_popup_time,
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));
  EXPECT_TRUE(db_->WritePopup(opener_site_2, popup_site,
                              /*access_id=*/456, second_popup_time,
                              /*is_current_interaction=*/true,
                              /*is_authentication_interaction=*/false));

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
class DIPSDatabaseQueryTest
    : public DIPSDatabaseTest,
      public testing::WithParamInterface<
          std::tuple<bool, content::DIPSTriggeringAction>> {
 public:
  using QueryMethod = base::RepeatingCallback<std::vector<std::string>(void)>;
  DIPSDatabaseQueryTest() : DIPSDatabaseTest(std::get<0>(GetParam())) {
    // Test with the prod feature's parameter to ensure the tested scenarios are
    // also valid/respected within prod env.
    features_.InitAndEnableFeature(features::kDIPS);
  }

  void SetUp() override {
    DIPSDatabaseTest::SetUp();
    grace_period = features::kDIPSGracePeriod.Get();
    interaction_ttl = features::kDIPSInteractionTtl.Get();
  }

  // Returns the DIPS-triggering action we're testing.
  content::DIPSTriggeringAction CurrentAction() {
    return std::get<1>(GetParam());
  }

  // Returns a callback for the respective querying method we want to test,
  // based on `features::kDIPSTriggeringAction`. This is equivalent to that
  // used by `DIPSStorage::GetSitesToClear` when the DIPS Timer fires.
  QueryMethod GetQueryMethodUnderTest() {
    switch (CurrentAction()) {
      case content::DIPSTriggeringAction::kNone:
        return base::BindLambdaForTesting(
            [&]() { return std::vector<std::string>{}; });
      case content::DIPSTriggeringAction::kBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBounced(grace_period); });
      case content::DIPSTriggeringAction::kStorage:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatUsedStorage(grace_period); });
      case content::DIPSTriggeringAction::kStatefulBounce:
        return base::BindLambdaForTesting(
            [&]() { return db_->GetSitesThatBouncedWithState(grace_period); });
    }
  }

  void WriteForCurrentAction(const std::string& site,
                             TimestampRange event_times,
                             TimestampRange interaction_times,
                             TimestampRange waa_times) {
    switch (CurrentAction()) {
      case content::DIPSTriggeringAction::kNone:
        break;
      case content::DIPSTriggeringAction::kBounce:
        db_->Write(site, /*storage_times=*/{}, interaction_times,
                   /*stateful_bounce_times=*/{},
                   /*bounce_times=*/event_times, waa_times);
        break;
      case content::DIPSTriggeringAction::kStorage:
        db_->Write(site, /*storage_times=*/event_times, interaction_times,
                   /*stateful_bounce_times=*/{}, /*bounce_times=*/{},
                   waa_times);
        break;
      case content::DIPSTriggeringAction::kStatefulBounce:
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

  base::Time event = Time::FromSecondsSinceUnixEpoch(1);
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

  base::Time interaction = Time::FromSecondsSinceUnixEpoch(1);
  TimestampRange interaction_times = {{interaction, interaction}};
  base::Time event = Time::FromSecondsSinceUnixEpoch(2);
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
    auto waa_time = Time::FromSecondsSinceUnixEpoch(100);
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
    EXPECT_EQ(db_->Read(site), std::nullopt);
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
  base::Time event = Time::FromSecondsSinceUnixEpoch(1);
  TimestampRange event_times = {{event, event}};
  base::Time interaction = Time::FromSecondsSinceUnixEpoch(4);
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
    auto event_time = Time::FromSecondsSinceUnixEpoch(100);
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
    EXPECT_EQ(db_->Read(site), std::nullopt);
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
  base::Time event = Time::FromSecondsSinceUnixEpoch(2);
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
  auto event_time = Time::FromSecondsSinceUnixEpoch(100);
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
  EXPECT_EQ(db_->Read(site), std::nullopt);
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByInteractionThenWaa) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Sets up an event with a interaction happening before the end of the event's
  // `grace_period` and an WAA some moments later:
  auto event_time = Time::FromSecondsSinceUnixEpoch(100);
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
  EXPECT_EQ(db_->Read(site), std::nullopt);
}

TEST_P(DIPSDatabaseQueryTest, ProtectedByWaaThenInteraction) {
  const QueryMethod query = GetQueryMethodUnderTest();
  const std::string site = "site.test";

  // Sets up an event with a WAA happening before the end of the event's
  // `grace_period` and an interaction some moments later:
  auto event_time = Time::FromSecondsSinceUnixEpoch(100);
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
  EXPECT_EQ(db_->Read(site), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DIPSDatabaseQueryTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(content::DIPSTriggeringAction::kBounce,
                          content::DIPSTriggeringAction::kStorage,
                          content::DIPSTriggeringAction::kStatefulBounce)));

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
        features::kDIPS,
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
                                  interaction_times->second,
                                  /*is_current_interaction=*/true,
                                  /*is_authentication_interaction=*/false));
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
    clock_.SetNow(Time::FromSecondsSinceUnixEpoch(100));
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
                                    times[(i + 1) % 3],
                                    /*is_current_interaction=*/true,
                                    /*is_authentication_interaction=*/false));
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
                                    times[(i + 1) % 3],
                                    /*is_current_interaction=*/true,
                                    /*is_authentication_interaction=*/false));
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
  base::Time storage = Time::FromSecondsSinceUnixEpoch(2);
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
    features_.InitAndEnableFeatureWithParameters(features::kDIPS,
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
             /*interaction_times=*/
             {{Time::FromSecondsSinceUnixEpoch(1),
               Time::FromSecondsSinceUnixEpoch(1)}},
             {}, {}, {});
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
  db_->Write("url1.test", {},
             /*interaction_times=*/
             {{Time::FromSecondsSinceUnixEpoch(1),
               Time::FromSecondsSinceUnixEpoch(1)}},
             {}, {}, {});
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

TEST_F(DIPSDatabaseHistogramTest, PerformanceMetrics) {
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.WriteTime", 0);
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.ReadTime", 0);

  // Write an entry to the db.
  db_->Write("url.test", {},
             /*interaction_times=*/
             {{Time::FromSecondsSinceUnixEpoch(1),
               Time::FromSecondsSinceUnixEpoch(1)}},
             {}, {}, {});
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.ReadTime", 0);
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.WriteTime", 1);

  // Read back the entry from the db.
  db_->Read("site.test");
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.ReadTime", 1);
  histograms().ExpectTotalCount("Privacy.DIPS.Database.Operation.WriteTime", 1);
}

class DIPSDatabaseInitializationTest : public testing::Test {
 public:
  DIPSDatabaseInitializationTest() {
    features_.InitAndEnableFeatureWithParameters(features::kDIPS,
                                                 {{"interaction_ttl", "inf"}});
  }

 protected:
  // Time columns root:
  const char* kStorageTimes = "site_storage";
  const char* kInteractionTimes = "user_interaction";
  const char* kStatefulBounceTimes = "stateful_bounce";
  const char* kStatelessBounceTimesV1 = "stateless_bounce";
  const char* kBounceTimesV2ToV3 = "bounce";

  void InitializeDatabase() { TestDatabase db(db_path_); }

  void ValidateSchemaAndMetadataMatchLatestVersion(sql::Database* db) {
    ValidateMetadataMatchesLatestVersion(db);

    ValidateBouncesTableMatchesLatestSchemaVersion(db);
    ValidatePopupsTableMatchesLatestSchemaVersion(db);
    ValidateConfigTableMatchesLatestSchemaVersion(db);
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

  std::string RowCount(sql::Database* db, const char* table) {
    return sql::test::ExecuteWithResult(
        db, base::StringPrintf("SELECT COUNT(*) FROM %s", table));
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

  void ValidateMetadataMatchesLatestVersion(sql::Database* db) {
    EXPECT_EQ(GetDatabaseVersion(db), DIPSDatabase::kLatestSchemaVersion);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(db),
              DIPSDatabase::kMinCompatibleSchemaVersion);
    // We no longer mark prepopulation in the meta table.
    EXPECT_EQ(GetPrepopulatedFromMetaTable(db), std::nullopt);
  }

  void ValidateBouncesTableMatchesLatestSchemaVersion(sql::Database* db) {
    EXPECT_TRUE(db->DoesTableExist("bounces"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "site"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "first_bounce_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "last_bounce_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "first_stateful_bounce_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "last_stateful_bounce_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "first_site_storage_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "last_site_storage_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "first_user_interaction_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "last_user_interaction_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "first_stateful_bounce_time"));
    EXPECT_TRUE(db->DoesColumnExist("bounces", "last_stateful_bounce_time"));
    EXPECT_TRUE(
        db->DoesColumnExist("bounces", "first_web_authn_assertion_time"));
    EXPECT_TRUE(
        db->DoesColumnExist("bounces", "last_web_authn_assertion_time"));
    // Expect obsolete and temporary columns to have been removed.
    EXPECT_FALSE(db->DoesColumnExist("bounces", "first_stateless_bounce_time"));
    EXPECT_FALSE(db->DoesColumnExist("bounces", "last_stateless_bounce_time"));
  }

  void ValidatePopupsTableMatchesLatestSchemaVersion(sql::Database* db) {
    EXPECT_TRUE(db->DoesTableExist("popups"));
    EXPECT_TRUE(db->DoesColumnExist("popups", "opener_site"));
    EXPECT_TRUE(db->DoesColumnExist("popups", "popup_site"));
    EXPECT_TRUE(db->DoesColumnExist("popups", "access_id"));
    EXPECT_TRUE(db->DoesColumnExist("popups", "last_popup_time"));
    EXPECT_TRUE(db->DoesColumnExist("popups", "is_current_interaction"));
  }

  void ValidateConfigTableMatchesLatestSchemaVersion(sql::Database* db) {
    EXPECT_TRUE(db->DoesTableExist("config"));
    EXPECT_TRUE(db->DoesColumnExist("config", "key"));
    EXPECT_TRUE(db->DoesColumnExist("config", "int_value"));
  }
};

TEST_F(DIPSDatabaseInitializationTest, InitializeEmptyDBWithLatestSchema) {
  // Initialize with an empty DB.
  InitializeDatabase();

  // Validate aspects of current schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ValidateSchemaAndMetadataMatchLatestVersion(&db);
  }
}

TEST_F(DIPSDatabaseInitializationTest, RazeIfIncompatible_TooNew) {
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
    EXPECT_EQ(GetPrepopulatedFromMetaTable(&db), v2sql_prepopulated);

    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, v2sql_version_num, v2sql_compatible_version_num));

    // Prepare simulation of raze if incompatible. by making this DB
    // incompatible.
    const int tiny_increment = 1;
    ASSERT_TRUE(meta_table.SetVersionNumber(DIPSDatabase::kLatestSchemaVersion +
                                            tiny_increment));
    ASSERT_TRUE(meta_table.SetCompatibleVersionNumber(
        DIPSDatabase::kLatestSchemaVersion + tiny_increment));

    EXPECT_EQ(GetDatabaseVersion(&db),
              DIPSDatabase::kLatestSchemaVersion + tiny_increment);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db),
              DIPSDatabase::kLatestSchemaVersion + tiny_increment);

    ASSERT_EQ(RowCount(&db, "bounces"), "4");
  }

  InitializeDatabase();

  // Verify post migration conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    // We should be on the latest schema version after razing.
    ValidateSchemaAndMetadataMatchLatestVersion(&db);

    // The DB was razed, so it shouldn't be marked as prepopulated, even though
    // it was marked as prepopulated before the raze.
    EXPECT_EQ(GetPrepopulatedFromConfigTable(&db), std::nullopt);

    // The raze should have deleted all existing data.
    EXPECT_EQ(RowCount(&db, "bounces"), "0");
  }
}

TEST_F(DIPSDatabaseInitializationTest, MigrateOldSchemaToLatestVersion) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase("v2.sql"));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(GetDatabaseVersion(&db), 2);
    EXPECT_EQ(GetDatabaseLastCompatibleVersion(&db), 2);
  }

  InitializeDatabase();

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    ValidateSchemaAndMetadataMatchLatestVersion(&db);
  }
}

// Verifies actions on the `config` table of the DIPS database.
class DIPSDatabaseConfigTest : public DIPSDatabaseTest {
 public:
  DIPSDatabaseConfigTest() : DIPSDatabaseTest(/*in_memory=*/true) {}
};

TEST_F(DIPSDatabaseConfigTest, GetUnknownKeyReturnsNullopt) {
  EXPECT_EQ(db_->GetConfigValueForTesting("test"), std::nullopt);
}

TEST_F(DIPSDatabaseConfigTest, WriteAndRead) {
  ASSERT_TRUE(db_->SetConfigValueForTesting("test", 42));
  EXPECT_THAT(db_->GetConfigValueForTesting("test"), Optional(42));
}

TEST_F(DIPSDatabaseConfigTest, Overwrite) {
  ASSERT_TRUE(db_->SetConfigValueForTesting("test", 42));
  ASSERT_TRUE(db_->SetConfigValueForTesting("test", 99));

  EXPECT_THAT(db_->GetConfigValueForTesting("test"), Optional(99));
}

TEST_F(DIPSDatabaseConfigTest, MultipleKeys) {
  ASSERT_TRUE(db_->SetConfigValueForTesting("foo", 42));
  ASSERT_TRUE(db_->SetConfigValueForTesting("bar", 99));

  EXPECT_THAT(db_->GetConfigValueForTesting("foo"), Optional(42));
  EXPECT_THAT(db_->GetConfigValueForTesting("bar"), Optional(99));
}

TEST_F(DIPSDatabaseConfigTest, TimerLastFired) {
  const base::Time time = Time::FromSecondsSinceUnixEpoch(1);

  ASSERT_EQ(db_->GetTimerLastFired(), std::nullopt);
  ASSERT_TRUE(db_->SetTimerLastFired(time));
  ASSERT_EQ(db_->GetTimerLastFired(), time);
}
