// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/storage_manager.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/structured/storage_manager_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/storage_manager.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {
using google::protobuf::RepeatedPtrField;

class TestStorageDelegate : public StorageManager::StorageDelegate {
 public:
  using FlushedCallback = base::RepeatingCallback<void(const FlushedKey&)>;

  TestStorageDelegate() = default;

  ~TestStorageDelegate() override = default;

  void SetFlushedCallback(FlushedCallback flushed) {
    flushed_callback_ = std::move(flushed);
  }

  void OnFlushed(const FlushedKey& key) override {
    flushed_count_ += 1;
    flushed_key_ = key;
    if (flushed_callback_) {
      flushed_callback_.Run(key);
    }
  }

  int flushed_count() const { return flushed_count_; }

  const std::optional<FlushedKey>& flushed_key() { return flushed_key_; }

 private:
  std::optional<FlushedKey> flushed_key_;
  int flushed_count_ = 0;

  FlushedCallback flushed_callback_;
};

StructuredEventProto BuildTestEvent(uint64_t id = 0,
                                    const std::vector<int64_t>& metrics = {}) {
  StructuredEventProto event;
  event.set_device_project_id(id);
  int metric_id = 0;
  for (int64_t metric : metrics) {
    auto* m = event.add_metrics();
    m->set_name_hash(metric_id++);
    m->set_value_int64(metric);
  }
  return event;
}

EventsProto BuildTestEventsProto(int num, int start_id = 0) {
  EventsProto events;
  for (int i = 0; i < num; ++i) {
    events.mutable_events()->Add(BuildTestEvent(start_id + i, {start_id}));
  }
  return events;
}

EventsProto ReadFlushedEvents(const base::FilePath& path) {
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(path, &content));
  EventsProto list;
  EXPECT_TRUE(list.MergeFromString(content));
  return list;
}

void WriteEventsProto(const base::FilePath& dir,
                      std::string_view name,
                      EventsProto&& events) {
  std::string content;
  ASSERT_TRUE(events.SerializeToString(&content));
  ASSERT_TRUE(base::WriteFile(dir.Append(name), content));
}

}  // namespace

class StorageManagerTest : public testing::Test {
 public:
  StorageManagerTest()
      : storage_delegate_(std::make_unique<TestStorageDelegate>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  base::FilePath GetArenaPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("arena_file"));
  }

  base::FilePath GetFlushDir() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("flushed_dir"));
  }

  std::unique_ptr<StorageManager> CreateManager(
      const StorageManagerConfig& config) {
    auto manager = std::make_unique<StorageManagerImpl>(config, GetArenaPath(),
                                                        GetFlushDir());
    manager->set_delegate(storage_delegate_.get());
    // Wait for FlushedMap initialization to complete.
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return manager->IsInitializedForTesting(); }));
    return manager;
  }

  void SortEvents(RepeatedPtrField<StructuredEventProto>& events) {
    std::ranges::sort(events, std::less<>(),
                      &StructuredEventProto::device_project_id);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

 protected:
  std::unique_ptr<TestStorageDelegate> storage_delegate_;
  TestingProfileManager profile_manager_;
};

TEST_F(StorageManagerTest, StoreAndProvideEventsInMemory) {
  auto manager =
      CreateManager({.buffer_max_bytes = 1024, .disk_max_bytes = 1024});

  // Add event.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3}));

  // Expect it to be recorded
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // Expect nothing to be flushed.
  EXPECT_EQ(base::ComputeDirectorySize(GetFlushDir()), 0l);

  // Provide events.
  base::test::TestFuture<RepeatedPtrField<StructuredEventProto>> future;
  manager->TakeEvents(future.GetCallback());
  auto events = future.Take();

  EXPECT_EQ(manager->RecordedEventsCount(), 0);
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].device_project_id(), 1ul);
}

TEST_F(StorageManagerTest, FlushEvents) {
  auto manager =
      CreateManager({.buffer_max_bytes = 512, .disk_max_bytes = 1024});

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(2, {1, 2, 3, 4}));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return storage_delegate_->flushed_count() == 1; }));

  ASSERT_TRUE(storage_delegate_->flushed_key().has_value());
  EventsProto events =
      ReadFlushedEvents(storage_delegate_->flushed_key()->path);
  ASSERT_EQ(events.events_size(), 1);
  const auto& event = events.events(0);
  EXPECT_EQ(event.device_project_id(), 1ul);
  EXPECT_EQ(event.metrics_size(), 4);
}

TEST_F(StorageManagerTest, FullBuffer) {
  auto manager =
      CreateManager({.buffer_max_bytes = 512, .disk_max_bytes = 1024});

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return storage_delegate_->flushed_count() == 1; }));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);
}

// Tests the ability of StorageManager to collect on-disk events and return them
// to be uploaded.
TEST_F(StorageManagerTest, ProvideFlushedEvents) {
  // This isn't needed in other tests because FlushedMap creates the directory
  // if it doesn't exist. Here we populate it before hand.
  ASSERT_TRUE(base::CreateDirectory(GetFlushDir()));
  WriteEventsProto(GetFlushDir(), "events1", BuildTestEventsProto(/*num=*/3));
  WriteEventsProto(GetFlushDir(), "events2",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/3));
  WriteEventsProto(GetFlushDir(), "events3",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/6));

  auto manager =
      CreateManager({.buffer_max_bytes = 512, .disk_max_bytes = 1024});
  // Returns the number of on-disk files if there are no events in memory.
  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  base::test::TestFuture<RepeatedPtrField<StructuredEventProto>> future;
  manager->TakeEvents(future.GetCallback());
  auto events = future.Take();

  // Wait for asynchronous deletions to complete.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events1"))) &&
           !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events2"))) &&
           !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events3")));
  }));

  // 3 events from each of the 3 files.
  ASSERT_EQ(events.size(), 9);
  // Sort the events to validating all events are present easier.
  SortEvents(events);

  uint64_t expected_id = 0;
  for (const auto& event : events) {
    EXPECT_EQ(event.device_project_id(), expected_id++);
  }
}

TEST_F(StorageManagerTest, ProvideFlushedAndInMemoryEvents) {
  // This isn't needed in other tests because FlushedMap creates the directory
  // if it doesn't exist. Here we populate it before hand.
  ASSERT_TRUE(base::CreateDirectory(GetFlushDir()));
  WriteEventsProto(GetFlushDir(), "events1", BuildTestEventsProto(/*num=*/3));
  WriteEventsProto(GetFlushDir(), "events2",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/3));
  WriteEventsProto(GetFlushDir(), "events3",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/6));

  auto manager =
      CreateManager({.buffer_max_bytes = 1024, .disk_max_bytes = 1024});
  manager->AddEvent(BuildTestEvent(9, {9}));
  manager->AddEvent(BuildTestEvent(10, {10}));
  manager->AddEvent(BuildTestEvent(11, {11}));

  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  base::test::TestFuture<RepeatedPtrField<StructuredEventProto>> future;
  manager->TakeEvents(future.GetCallback());
  auto events = future.Take();

  // Wait for asynchronous deletions to complete.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events1"))) &&
           !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events2"))) &&
           !base::PathExists(
               GetFlushDir().Append(FILE_PATH_LITERAL("events3")));
  }));

  // 3 events from each of the 3 files and 3 in-memory events.
  ASSERT_EQ(events.size(), 12);
  // Sort the events to validating all events are present easier.
  SortEvents(events);

  uint64_t expected_id = 0;
  for (const auto& event : events) {
    EXPECT_EQ(event.device_project_id(), expected_id++);
  }
}

TEST_F(StorageManagerTest, Purge) {
  // This isn't needed in other tests because FlushedMap creates the directory
  // if it doesn't exist. Here we populate it before hand.
  ASSERT_TRUE(base::CreateDirectory(GetFlushDir()));
  WriteEventsProto(GetFlushDir(), "events1", BuildTestEventsProto(/*num=*/3));
  WriteEventsProto(GetFlushDir(), "events2",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/3));
  WriteEventsProto(GetFlushDir(), "events3",
                   BuildTestEventsProto(/*num=*/3, /*start_id=*/6));

  auto manager =
      CreateManager({.buffer_max_bytes = 1024, .disk_max_bytes = 1024});
  manager->AddEvent(BuildTestEvent(9, {9}));
  manager->AddEvent(BuildTestEvent(10, {10}));
  manager->AddEvent(BuildTestEvent(11, {11}));

  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  manager->Purge();

  base::ThreadPoolInstance::Get()->FlushForTesting();

  ASSERT_EQ(manager->RecordedEventsCount(), 0);

  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events1"))));
  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events2"))));
  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events3"))));

  EXPECT_FALSE(base::PathExists(GetArenaPath()));
}

TEST_F(StorageManagerTest, PurgeUninitialized) {
  // This isn't needed in other tests because FlushedMap creates the directory
  // if it doesn't exist. Here we populate it before hand.
  ASSERT_TRUE(base::CreateDirectory(GetFlushDir()));
  WriteEventsProto(GetFlushDir(), "events1", BuildTestEventsProto(/*num=*/3));

  // Create manager but don't Wait() for it to initialize FlushedMap yet.
  auto manager = std::make_unique<StorageManagerImpl>(
      StorageManagerConfig{.buffer_max_bytes = 1024, .disk_max_bytes = 1024},
      GetArenaPath(), GetFlushDir());
  manager->set_delegate(storage_delegate_.get());

  manager->Purge();

  // Now wait for initialization and subsequent purge.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsInitializedForTesting(); }));

  // Wait for asynchronous deletions to complete.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_EQ(manager->RecordedEventsCount(), 0);
  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events1"))));
}

TEST_F(StorageManagerTest, FlushedQuotaExceeded) {
  auto manager = CreateManager({.buffer_max_bytes = 512, .disk_max_bytes = 64});

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  // Expect 1 batch of events to be written to disk.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return storage_delegate_->flushed_count() == 1; }));
  // Only the previously added event should be represented as recorded.
  EXPECT_EQ(manager->RecordedEventsCount(), 1);
  FlushedKey previous_key = *storage_delegate_->flushed_key();

  // A flush should occur but our quota has been reached.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));

  // Wait for asynchronous flush and subsequent deletion to complete.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // After the second flush, the first file should be deleted because of quota.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    return !base::PathExists(previous_key.path);
  }));
}

TEST_F(StorageManagerTest, ProvideEventsRaceCondition) {
  // This test simulates a race condition where a new event is flushed to disk
  // while TakeEvents() is already in progress reading from disk on a
  // background thread.

  ASSERT_TRUE(base::CreateDirectory(GetFlushDir()));
  WriteEventsProto(GetFlushDir(), "events1", BuildTestEventsProto(/*num=*/1));

  auto manager =
      CreateManager({.buffer_max_bytes = 512, .disk_max_bytes = 1024});

  // 1 on-disk file.
  ASSERT_EQ(manager->RecordedEventsCount(), 1);

  base::test::TestFuture<RepeatedPtrField<StructuredEventProto>> future;
  manager->TakeEvents(future.GetCallback());

  // While TakeEvents() background task is likely running (or enqueued),
  // add a new event that will be flushed to disk.
  // We use a small buffer size (512) and add two large events to trigger a
  // flush.
  manager->AddEvent(BuildTestEvent(2, {1, 2, 3, 4, 5, 6, 7, 8}));
  manager->AddEvent(BuildTestEvent(3, {1, 2, 3, 4, 5, 6, 7, 8}));

  // Wait for the flush to complete and the TakeEvents() to finish.
  auto events = future.Take();

  // TakeEvents should have returned the events from "events1".
  // (It might also return the newly flushed events if the flush happened
  // before ReadEventsOnBackgroundThread grabbed the keys, but in this test
  // setup we want to verify that the newly flushed events ARE NOT deleted
  // if they weren't part of the 'take').
  // Given how TakeEvents is implemented, it grabs flushed_map_.keys() ON THE
  // UI THREAD before posting the background task. So "events2" (newly
  // flushed) won't be in that list.

  // The newly flushed file should NOT have been deleted by TakeEvents
  // completion. RecordedEventsCount() should now be 1 (representing the newly
  // flushed file).
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // TakeEvents() should have read 1 event.
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].device_project_id(), 0ul);
}

}  // namespace metrics::structured
