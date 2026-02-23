// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/recording_data_manager_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

namespace {

using ::base::test::EqualsProto;
using ::testing::Optional;

class RecordingDataManagerImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    RecreateCache();
  }

  void TearDown() override {
    data_manager_.reset();
    db_provider_.reset();
    // Wait for destruction on a different sequence.
    WaitForDatabaseOperations();
  }

  // Simulates restart of the browser by recreating the cache.
  void RecreateCache() {
    // Process remaining operations.
    WaitForDatabaseOperations();
    data_manager_ = std::make_unique<RecordingDataManagerImpl>(
        db_provider_.get(), temp_dir_.GetPath());
    // Wait until database has loaded.
    WaitForDatabaseOperations();
  }

  void WaitForDatabaseOperations() { task_environment_.RunUntilIdle(); }

  RecordingDataManagerImpl& data_manager() { return *data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<RecordingDataManagerImpl> data_manager_;
};

// Tests that recordings can be added and retrieved from the database.
TEST_F(RecordingDataManagerImplTest, AddAndGetRecording) {
  const Recording r1 = [] {
    Recording r;
    r.set_url("https://foo.com");
    r.set_start_time(123);
    Recording::Action* action = r.add_actions();
    action->set_delta(8);
    action->set_element_selector("#foo");
    action->mutable_click_specifics();
    return r;
  }();
  const Recording r2 = [] {
    Recording r;
    r.set_url("https://bar.com");
    r.set_start_time(456);
    Recording::Action* action = r.add_actions();
    action->set_delta(9);
    action->set_element_selector("#bar");
    action->mutable_click_specifics();
    return r;
  }();

  EXPECT_FALSE(data_manager().GetRecording("https://foo.com"));
  EXPECT_FALSE(data_manager().GetRecording("https://bar.com"));

  data_manager().AddRecording(r1);
  WaitForDatabaseOperations();

  EXPECT_THAT(data_manager().GetRecording("https://foo.com"),
              Optional(EqualsProto(r1)));
  EXPECT_FALSE(data_manager().GetRecording("https://bar.com"));

  data_manager().AddRecording(r2);
  WaitForDatabaseOperations();

  EXPECT_THAT(data_manager().GetRecording("https://foo.com"),
              Optional(EqualsProto(r1)));
  EXPECT_THAT(data_manager().GetRecording("https://bar.com"),
              Optional(EqualsProto(r2)));
}

TEST_F(RecordingDataManagerImplTest, AddRecordingWithName) {
  Recording r;
  r.set_url("https://foo.com");
  r.set_name("My Recording");

  data_manager().AddRecording(r);

  {
    auto recording = data_manager().GetRecording("https://foo.com");
    ASSERT_TRUE(recording.has_value());
    EXPECT_EQ(recording->name(), "My Recording");
  }

  RecreateCache();
  auto recording = data_manager().GetRecording("https://foo.com");
  ASSERT_TRUE(recording.has_value());
  EXPECT_EQ(recording->name(), "My Recording");
}

TEST_F(RecordingDataManagerImplTest, AddRecordingWithScreenshot) {
  Recording r;
  r.set_url("https://foo.com");
  r.set_screenshot("fake_screenshot_data");

  data_manager().AddRecording(r);

  {
    auto recording = data_manager().GetRecording("https://foo.com");
    ASSERT_TRUE(recording.has_value());
    EXPECT_EQ(recording->screenshot(), "fake_screenshot_data");
  }

  RecreateCache();
  auto recording = data_manager().GetRecording("https://foo.com");
  ASSERT_TRUE(recording.has_value());
  EXPECT_EQ(recording->screenshot(), "fake_screenshot_data");
}

// Tests that RecordingDataManagerImpl handles a race condition in LevelDB:
// TODO(crbug.com/483687781): Remove this test once the the issue fixed.
TEST(RecordingDataManagerImplTest_LevelDbRaceCondition, AvoidDcheck) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto db_provider = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
      temp_dir.GetPath());
  auto data_manager = std::make_unique<RecordingDataManagerImpl>(
      db_provider.get(), temp_dir.GetPath());
  for (int i = 0; i < 12; ++i) {
    data_manager->AddRecording(Recording());
  }
  task_environment.RunUntilIdle();
}

}  // namespace

}  // namespace record_replay
