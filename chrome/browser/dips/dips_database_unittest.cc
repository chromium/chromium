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

class DIPSDatabaseTest : public testing::TestWithParam<bool> {
 public:
  DIPSDatabaseTest() = default;

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DIPSDatabase> db_;

 private:
  // Test setup.
  void SetUp() override {
    if (GetParam()) {
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
    if (!GetParam())
      ASSERT_TRUE(temp_dir_.Delete());
  }
};

INSTANTIATE_TEST_SUITE_P(All, DIPSDatabaseTest, testing::Bool());

// Test adding, updating, querying, and deleting entries in the bounces table
// in the DIPSDatabase.
TEST_P(DIPSDatabaseTest, AddUpdateQueryDeleteBounce) {
  // Add a bounce for site1.
  const std::string site1 = GetSiteForDIPS(GURL("http://www.youtube.com/"));
  absl::optional<base::Time> storage_time1 = Time::FromDoubleT(1);
  EXPECT_TRUE(db_->Write(site1, storage_time1, absl::nullopt));

  // Add a bounce for site2.
  const std::string site2 = GetSiteForDIPS(GURL("http://mail.google.com/"));
  absl::optional<base::Time> interaction_time2 = Time::FromDoubleT(2);
  EXPECT_TRUE(db_->Write(site2, absl::nullopt, interaction_time2));

  // Query both of them.
  absl::optional<StateValue> state1 = db_->Read(site1);
  ASSERT_TRUE(state1.has_value());
  EXPECT_EQ(state1->first_site_storage_time, storage_time1);
  EXPECT_FALSE(state1->first_user_interaction_time.has_value());

  absl::optional<StateValue> state2 = db_->Read(site2);
  ASSERT_TRUE(state2.has_value());
  EXPECT_FALSE(state2->first_site_storage_time.has_value());
  EXPECT_EQ(state2->first_user_interaction_time, interaction_time2);

  // Update the second.
  absl::optional<base::Time> storage_time2 = Time::FromDoubleT(3);
  state2->first_site_storage_time = storage_time2;
  EXPECT_TRUE(db_->Write(site2, state2->first_site_storage_time,
                         state2->first_user_interaction_time));
  // Query the second again.
  absl::optional<StateValue> updated_state2 = db_->Read(site2);
  ASSERT_TRUE(updated_state2.has_value());
  EXPECT_EQ(updated_state2->first_site_storage_time, storage_time2);
  EXPECT_EQ(updated_state2->first_user_interaction_time, interaction_time2);

  //  Delete the first.
  EXPECT_EQ(db_->RemoveRow(site1), true);

  // Query the first one again, making sure there is no state now.
  EXPECT_FALSE(db_->Read(site1).has_value());

  // Query a site that never had DIPS State.
  const std::string site3 = GetSiteForDIPS(GURL("https://www.waze.com/"));
  EXPECT_FALSE(db_->Read(site3).has_value());
}
