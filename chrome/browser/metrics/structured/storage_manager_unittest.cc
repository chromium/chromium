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
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
  using DeletedCallback =
      base::RepeatingCallback<void(const FlushedKey&, DeleteReason)>;

  TestStorageDelegate() = default;

  ~TestStorageDelegate() override = default;

  void SetFlushedCallback(FlushedCallback flushed) {
    flushed_callback_ = std::move(flushed);
  }

  void SetDeletedCallback(DeletedCallback deleted) {
    delete_callback_ = std::move(deleted);
  }

  void OnFlushed(const FlushedKey& key) override {
    flushed_count_ += 1;
    flushed_key_ = key;
    if (flushed_callback_) {
      flushed_callback_.Run(key);
    }
  }

  void OnDeleted(const FlushedKey& key, DeleteReason reason) override {
    delete_count_ += 1;
    if (delete_callback_) {
      delete_callback_.Run(key, reason);
    }
  }

  int flushed_count() const { return flushed_count_; }

  const std::optional<FlushedKey>& flushed_key() { return flushed_key_; }

  int delete_count() const { return delete_count_; }

 private:
  std::optional<FlushedKey> flushed_key_;
  int flushed_count_ = 0;
  int delete_count_ = 0;

  FlushedCallback flushed_callback_;
  DeletedCallback delete_callback_;
};

StructuredEventProto BuildTestEvent(
    uint64_t id = 0,
    const std::vector<int64_t>& metrics = std::vector<int64_t>()) {
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

StorageManagerConfig CreateManagerConfig(int32_t buffer_max_bytes,
                                         int32_t disk_max_bytes) {
  return StorageManagerConfig{
      .buffer_max_bytes = buffer_max_bytes,
      .disk_max_bytes = disk_max_bytes,
  };
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

  void Wait() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<StorageManager> CreateManager(
      const StorageManagerConfig& config) {
    auto manager = std::make_unique<StorageManagerImpl>(config, GetArenaPath(),
                                                        GetFlushDir());
    manager->set_delegate(storage_delegate_.get());
    return manager;
  }

  void SortEvents(RepeatedPtrField<StructuredEventProto>& events) {
    std::sort(events.begin(), events.end(),
              [](const StructuredEventProto& l,
                 const StructuredEventProto& r) -> bool {
                return l.device_project_id() < r.device_project_id();
              });
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  base::ScopedTempDir temp_dir_;

 protected:
  std::unique_ptr<TestStorageDelegate> storage_delegate_;
  TestingProfileManager profile_manager_;
};

TEST_F(StorageManagerTest, StoreAndProvideEventsInMemory) {
  std::unique_ptr<StorageManager> manager = CreateManager(
      CreateManagerConfig(/*buffer_max_bytes=*/1024, /*disk_max_bytes=*/1024));
  Wait();

  // Add event.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3}));

  // Expect it to be recorded
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // Expect nothing to be flushed.
  EXPECT_EQ(base::ComputeDirectorySize(GetFlushDir()), 0l);

  // Provide events.
  RepeatedPtrField<StructuredEventProto> events = manager->TakeEvents();

  EXPECT_EQ(manager->RecordedEventsCount(), 0);
  EXPECT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].device_project_id(), 1ul);
}

TEST_F(StorageManagerTest, FlushEvents) {
  std::unique_ptr<StorageManager> manager = CreateManager(
      CreateManagerConfig(/*buffer_max_bytes=*/512, /*disk_max_bytes=*/1024));
  Wait();

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(2, {1, 2, 3, 4}));
  Wait();
  ASSERT_EQ(storage_delegate_->flushed_count(), 1);

  ASSERT_TRUE(storage_delegate_->flushed_key().has_value());
  EventsProto events =
      ReadFlushedEvents(storage_delegate_->flushed_key()->path);
  ASSERT_EQ(events.events_size(), 1);
  const auto& event = events.events(0);
  EXPECT_EQ(event.device_project_id(), 1ul);
  EXPECT_EQ(event.metrics_size(), 4);
}

TEST_F(StorageManagerTest, FullBuffer) {
  std::unique_ptr<StorageManager> manager = CreateManager(
      CreateManagerConfig(/*buffer_max_bytes=*/512, /*disk_max_bytes=*/1024));
  Wait();

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  Wait();
  EXPECT_EQ(manager->RecordedEventsCount(), 1);
  EXPECT_EQ(storage_delegate_->flushed_count(), 1);
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

  std::unique_ptr<StorageManager> manager =
      CreateManager(CreateManagerConfig(/*buffer_max_bytes=*/512,
                                        /*disk_max_bytes=*/1024));
  Wait();
  // Returns the number of on-disk files if there are no events in memory.
  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  storage_delegate_->SetDeletedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key, DeleteReason reason) {
        EXPECT_EQ(reason, DeleteReason::kUploaded);
        EXPECT_FALSE(base::PathExists(key.path));
      }));

  RepeatedPtrField<StructuredEventProto> events = manager->TakeEvents();
  Wait();

  // Since on-disk events are being read, it is expected they are deleted.
  EXPECT_EQ(storage_delegate_->delete_count(), 3);
  // 3 events from each of the 3 files.
  EXPECT_EQ(events.size(), 9);
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

  std::unique_ptr<StorageManager> manager =
      CreateManager(CreateManagerConfig(/*buffer_max_bytes=*/1024,
                                        /*disk_max_bytes=*/1024));
  Wait();
  manager->AddEvent(BuildTestEvent(9, {9}));
  manager->AddEvent(BuildTestEvent(10, {10}));
  manager->AddEvent(BuildTestEvent(11, {11}));

  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  storage_delegate_->SetDeletedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key, DeleteReason reason) {
        EXPECT_EQ(reason, DeleteReason::kUploaded);
        EXPECT_FALSE(base::PathExists(key.path));
      }));

  RepeatedPtrField<StructuredEventProto> events = manager->TakeEvents();
  Wait();

  // Since on-disk events are being read, it is expected they are deleted.
  EXPECT_EQ(storage_delegate_->delete_count(), 3);
  // 3 events from each of the 3 files and 3 in-memory events.
  EXPECT_EQ(events.size(), 12);
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

  std::unique_ptr<StorageManager> manager =
      CreateManager(CreateManagerConfig(/*buffer_max_bytes=*/1024,
                                        /*disk_max_bytes=*/1024));
  Wait();
  manager->AddEvent(BuildTestEvent(9, {9}));
  manager->AddEvent(BuildTestEvent(10, {10}));
  manager->AddEvent(BuildTestEvent(11, {11}));

  ASSERT_EQ(manager->RecordedEventsCount(), 3);

  manager->Purge();

  ASSERT_EQ(manager->RecordedEventsCount(), 0);

  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events1"))));
  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events2"))));
  EXPECT_FALSE(
      base::PathExists(GetFlushDir().Append(FILE_PATH_LITERAL("events3"))));

  int64_t size = 0;
  EXPECT_TRUE(base::GetFileSize(GetArenaPath(), &size));
  EXPECT_EQ(size, 0l);
}

TEST_F(StorageManagerTest, FlushedQuotaExceeded) {
  std::unique_ptr<StorageManager> manager = CreateManager(
      CreateManagerConfig(/*buffer_max_bytes=*/512, /*disk_max_bytes=*/64));
  Wait();

  storage_delegate_->SetFlushedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key) { EXPECT_TRUE(base::PathExists(key.path)); }));

  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  EXPECT_EQ(manager->RecordedEventsCount(), 1);

  // A flush should occur and the event should be added.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  Wait();
  // Only the previously added event should be represented as recorded.
  EXPECT_EQ(manager->RecordedEventsCount(), 1);
  // Expect 1 batch of events to be written to disk.
  EXPECT_EQ(storage_delegate_->flushed_count(), 1);

  FlushedKey previous_key = *storage_delegate_->flushed_key();

  std::vector<FlushedKey> dropped_keys;

  storage_delegate_->SetDeletedCallback(base::BindLambdaForTesting(
      [&](const FlushedKey& key, DeleteReason reason) {
        EXPECT_EQ(reason, DeleteReason::kExceededQuota);
        // The path that was flushed first (only 2) should be the key that is
        // flushed. Only one is expected so to be deleted, if more happen this
        // will fail.
        EXPECT_EQ(previous_key.path, key.path);
        dropped_keys.push_back(key);
      }));

  // A flush should occur but our quota has been reached.
  manager->AddEvent(BuildTestEvent(1, {1, 2, 3, 4}));
  Wait();

  // After the second flush, the first file should be deleted.
  EXPECT_EQ(storage_delegate_->delete_count(), 1);

  // Deleting of the files is asynchronous, checking once all tasks have
  // completed.
  for (const FlushedKey& key : dropped_keys) {
    EXPECT_FALSE(base::PathExists(key.path));
  }
}

}  // namespace metrics::structured
