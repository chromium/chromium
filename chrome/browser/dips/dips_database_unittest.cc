// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "chrome/browser/dips/dips_database.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/dips/dips_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::Time;

class DIPSDatabase;

namespace {
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
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DIPSDatabase> db_;

 private:
  // Test setup.
  void SetUp() override {
    if (in_memory_) {
      db_ = std::make_unique<DIPSDatabase>(absl::nullopt);
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      db_ = std::make_unique<DIPSDatabase>(
          temp_dir_.GetPath().AppendASCII("DIPS.db"));
    }

    ASSERT_EQ(db_->Init(), sql::INIT_OK);
  }

  void TearDown() override {
    db_.reset();

    // Deletes temporary directory from on-disk tests
    if (!in_memory_)
      ASSERT_TRUE(temp_dir_.Delete());
  }

  bool in_memory_;
};

// A test class that lets us ensure that we can add, update, and delete bounces
// for all columns in the DIPSDatabase.
// Parameterized over whether the db is in memory, and what column we're
// testing.
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

// Test querying the `bounces` table of the DIPSDatabase.
TEST_P(DIPSDatabaseAllColumnTest, QueryBounce) {
  // Add a bounce for site.
  const std::string site = GetSiteForDIPS(GURL("https://example.test"));

  TimestampRange bounce({Time::FromDoubleT(1), Time::FromDoubleT(1)});
  EXPECT_TRUE(WriteToVariableColumn(site, bounce));
  EXPECT_EQ(ReadValueForVariableColumn(db_->Read(site)), bounce);

  // Query a site that never had DIPS State, verifying
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
