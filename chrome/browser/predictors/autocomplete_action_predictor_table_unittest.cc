// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "sql/statement.h"

#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using predictors::AutocompleteActionPredictorTable;

namespace predictors {

class AutocompleteActionPredictorTableTest : public testing::Test {
 public:
  AutocompleteActionPredictorTableTest();
  ~AutocompleteActionPredictorTableTest() override;

  void SetUp() override;
  void TearDown() override;

  size_t CountRecords() const;

  void AddAll();

  bool RowsAreEqual(const AutocompleteActionPredictorTable::Row& lhs,
                    const AutocompleteActionPredictorTable::Row& rhs) const;

  TestingProfile* profile() { return &profile_; }

 protected:
  // Test functions that can be run against this text fixture or
  // AutocompleteActionPredictorTableReopenTest that inherits from this.
  void TestGetRow();
  void TestAddAndUpdateRows();
  void TestDeleteRows();
  void TestDeleteAllRows();

  AutocompleteActionPredictorTable::Rows test_db_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> db_;
};

class AutocompleteActionPredictorTableReopenTest
    : public AutocompleteActionPredictorTableTest {
 public:
  void SetUp() override {
    // By calling SetUp twice, we make sure that the table already exists for
    // this fixture.
    AutocompleteActionPredictorTableTest::SetUp();
    AutocompleteActionPredictorTableTest::TearDown();
    AutocompleteActionPredictorTableTest::SetUp();
  }
};

AutocompleteActionPredictorTableTest::AutocompleteActionPredictorTableTest() {}

AutocompleteActionPredictorTableTest::~AutocompleteActionPredictorTableTest() {
}

void AutocompleteActionPredictorTableTest::SetUp() {
  db_ = std::make_unique<PredictorDatabase>(
      &profile_, base::SequencedTaskRunner::GetCurrentDefault());
  content::RunAllTasksUntilIdle();

  test_db_.push_back(AutocompleteActionPredictorTable::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", u"goog",
      GURL("http://www.google.com/"), 1, 0));
  test_db_.push_back(AutocompleteActionPredictorTable::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", u"slash",
      GURL("http://slashdot.org/"), 3, 2));
  test_db_.push_back(AutocompleteActionPredictorTable::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", u"news",
      GURL("http://slashdot.org/"), 0, 1));
}

void AutocompleteActionPredictorTableTest::TearDown() {
  db_ = nullptr;
  content::RunAllTasksUntilIdle();
  test_db_.clear();
}

size_t AutocompleteActionPredictorTableTest::CountRecords() const {
  sql::Statement s(db_->GetDatabase()->GetUniqueStatement(
      "SELECT count(*) FROM network_action_predictor"));
  EXPECT_TRUE(s.Step());
  return static_cast<size_t>(s.ColumnInt(0));
}

void AutocompleteActionPredictorTableTest::AddAll() {
  db_->autocomplete_table()->AddAndUpdateRows(
      test_db_, AutocompleteActionPredictorTable::Rows());

  EXPECT_EQ(test_db_.size(), CountRecords());
}

bool AutocompleteActionPredictorTableTest::RowsAreEqual(
    const AutocompleteActionPredictorTable::Row& lhs,
    const AutocompleteActionPredictorTable::Row& rhs) const {
  return (lhs.id == rhs.id &&
          lhs.user_text == rhs.user_text &&
          lhs.url == rhs.url &&
          lhs.number_of_hits == rhs.number_of_hits &&
          lhs.number_of_misses == rhs.number_of_misses);
}

void AutocompleteActionPredictorTableTest::TestGetRow() {
  db_->autocomplete_table()->AddAndUpdateRows(
      AutocompleteActionPredictorTable::Rows(1, test_db_[0]),
      AutocompleteActionPredictorTable::Rows());
  AutocompleteActionPredictorTable::Row row;
  db_->autocomplete_table()->GetRow(test_db_[0].id, &row);
  EXPECT_TRUE(RowsAreEqual(test_db_[0], row))
      << "Expected: Row with id " << test_db_[0].id << "\n"
      << "Got:      Row with id " << row.id;
}

void AutocompleteActionPredictorTableTest::TestAddAndUpdateRows() {
  EXPECT_EQ(0U, CountRecords());

  AutocompleteActionPredictorTable::Rows rows_to_add;
  rows_to_add.push_back(test_db_[0]);
  rows_to_add.push_back(test_db_[1]);
  db_->autocomplete_table()->AddAndUpdateRows(
      rows_to_add,
      AutocompleteActionPredictorTable::Rows());
  EXPECT_EQ(2U, CountRecords());

  AutocompleteActionPredictorTable::Row row1 = test_db_[1];
  row1.number_of_hits = row1.number_of_hits + 1;
  db_->autocomplete_table()->AddAndUpdateRows(
      AutocompleteActionPredictorTable::Rows(1, test_db_[2]),
      AutocompleteActionPredictorTable::Rows(1, row1));
  EXPECT_EQ(3U, CountRecords());

  AutocompleteActionPredictorTable::Row updated_row1;
  db_->autocomplete_table()->GetRow(test_db_[1].id, &updated_row1);
  EXPECT_TRUE(RowsAreEqual(row1, updated_row1))
      << "Expected: Row with id " << row1.id << "\n"
      << "Got:      Row with id " << updated_row1.id;

  AutocompleteActionPredictorTable::Row row0 = test_db_[0];
  row0.number_of_hits = row0.number_of_hits + 2;
  AutocompleteActionPredictorTable::Row row2 = test_db_[2];
  row2.number_of_hits = row2.number_of_hits + 2;
  AutocompleteActionPredictorTable::Rows rows_to_update;
  rows_to_update.push_back(row0);
  rows_to_update.push_back(row2);
  db_->autocomplete_table()->AddAndUpdateRows(
      AutocompleteActionPredictorTable::Rows(),
      rows_to_update);
  EXPECT_EQ(3U, CountRecords());

  AutocompleteActionPredictorTable::Row updated_row0, updated_row2;
  db_->autocomplete_table()->GetRow(test_db_[0].id, &updated_row0);
  db_->autocomplete_table()->GetRow(test_db_[2].id, &updated_row2);
  EXPECT_TRUE(RowsAreEqual(row0, updated_row0))
      << "Expected: Row with id " << row0.id << "\n"
      << "Got:      Row with id " << updated_row0.id;
  EXPECT_TRUE(RowsAreEqual(row2, updated_row2))
      << "Expected: Row with id " << row2.id << "\n"
      << "Got:      Row with id " << updated_row2.id;
}

void AutocompleteActionPredictorTableTest::TestDeleteRows() {
  AddAll();
  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  id_list.push_back(test_db_[0].id);
  id_list.push_back(test_db_[2].id);
  db_->autocomplete_table()->DeleteRows(id_list);
  EXPECT_EQ(test_db_.size() - 2, CountRecords());

  AutocompleteActionPredictorTable::Row row;
  db_->autocomplete_table()->GetRow(test_db_[1].id, &row);
  EXPECT_TRUE(RowsAreEqual(test_db_[1], row));
}

void AutocompleteActionPredictorTableTest::TestDeleteAllRows() {
  AddAll();
  db_->autocomplete_table()->DeleteAllRows();
  EXPECT_EQ(0U, CountRecords());
}

// AutocompleteActionPredictorTableTest tests
TEST_F(AutocompleteActionPredictorTableTest, GetRow) {
  TestGetRow();
}

TEST_F(AutocompleteActionPredictorTableTest, AddAndUpdateRows) {
  TestAddAndUpdateRows();
}

TEST_F(AutocompleteActionPredictorTableTest, DeleteRows) {
  TestDeleteRows();
}

TEST_F(AutocompleteActionPredictorTableTest, DeleteAllRows) {
  TestDeleteAllRows();
}

// AutocompleteActionPredictorTableReopenTest tests
TEST_F(AutocompleteActionPredictorTableReopenTest, GetRow) {
  TestGetRow();
}

TEST_F(AutocompleteActionPredictorTableReopenTest, AddAndUpdateRows) {
  TestAddAndUpdateRows();
}

TEST_F(AutocompleteActionPredictorTableReopenTest, DeleteRows) {
  TestDeleteRows();
}

TEST_F(AutocompleteActionPredictorTableReopenTest, DeleteAllRows) {
  TestDeleteAllRows();
}

}  // namespace predictors
