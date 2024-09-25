// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/arena_event_buffer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {

using ::google::protobuf::RepeatedPtrField;

// Creates an event for testing. The serialized size of this event is about 9
// bytes.
StructuredEventProto TestEvent(uint64_t id) {
  StructuredEventProto event;
  event.set_device_project_id(id);
  return event;
}

EventsProto ReadEvents(const base::FilePath& path) {
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(path, &content));
  EventsProto proto;
  EXPECT_TRUE(proto.MergeFromString(content));
  return proto;
}

}  // namespace

class ArenaEventBufferTest : public testing::Test {
 public:
  const base::TimeDelta kWriteDelay = base::Seconds(0);

  ArenaEventBufferTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  base::FilePath GetPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("proto_file"));
  }

  base::FilePath GetAltPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("alt_proto_file"));
  }

  std::unique_ptr<ArenaEventBuffer> CreateTestBuffer(uint64_t max_size) {
    return std::make_unique<ArenaEventBuffer>(GetPath(), kWriteDelay, max_size);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  TestingProfile* AddProfile() {
    return profile_manager_.CreateTestingProfile("p1");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  TestingProfileManager profile_manager_;
};

TEST_F(ArenaEventBufferTest, OkEvent) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
}

TEST_F(ArenaEventBufferTest, FullEvent) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  // Create an event that is larger then the heuristic.
  auto event2 = TestEvent(2);

  // Add 10 metrics
  for (int i = 0; i < 10; ++i) {
    auto* metric = event2.add_metrics();
    metric->set_name_hash(i);
    metric->set_value_string("metric value");
  }

  EXPECT_EQ(buffer->AddEvent(event2), Result::kFull);
}

TEST_F(ArenaEventBufferTest, Purge) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  buffer->Purge();
  Wait();

  EXPECT_EQ(buffer->resource_info().used_size_bytes, 0ul);

  EXPECT_FALSE(base::PathExists(GetPath()));
}

TEST_F(ArenaEventBufferTest, UpdatePath) {
  EventsProto events;
  events.mutable_events()->Add(TestEvent(2));

  std::string content;
  ASSERT_TRUE(events.SerializeToString(&content));
  ASSERT_TRUE(base::WriteFile(GetAltPath(), content));

  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();
  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->proto()->events_size(), 1);

  buffer->UpdatePath(GetAltPath());
  Wait();
  EXPECT_EQ(buffer->proto()->events_size(), 2);

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->proto()->events_size(), 3);
}

TEST_F(ArenaEventBufferTest, PeriodicEventBackup) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(kEventStorageManager,
                                              {{
                                                  "event_backup_time_s",
                                                  "3"  // seconds
                                              }});
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  // Add an event.
  buffer->AddEvent(TestEvent(1));
  EXPECT_EQ(buffer->proto()->events_size(), 1);

  // Wait for 3 seconds for the timer to trigger a backup.
  task_environment_.FastForwardBy(base::Seconds(3));
  Wait();

  // Read the events from disk to see if the file has the expected content.
  EventsProto events = ReadEvents(GetPath());
  ASSERT_EQ(events.events_size(), 1);
  const auto& event = events.events(0);
  EXPECT_EQ(event.device_project_id(), 1ul);
}

TEST_F(ArenaEventBufferTest, Serialize) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(2)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(3)), Result::kOk);

  RepeatedPtrField<StructuredEventProto> events = buffer->Serialize();

  // Expect |events| to not be associated with an arena.
  EXPECT_EQ(events.GetArena(), nullptr);

  ASSERT_EQ(events.size(), 3);
  for (size_t i = 0; i < static_cast<size_t>(events.size()); ++i) {
    EXPECT_EQ(events[i].device_project_id(), i + 1);
  }

  // Serialize is a copy.
  EXPECT_EQ(buffer->proto()->events_size(), 3);
}

TEST_F(ArenaEventBufferTest, Flush) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(2)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(3)), Result::kOk);

  buffer->proto().QueueWrite();
  Wait();

  const base::FilePath& path = GetPath();
  base::File::Info info;
  CHECK(base::GetFileInfo(path, &info));

  const base::FilePath new_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("new_proto_file"));

  bool flushed = false;
  buffer->Flush(new_path, base::BindLambdaForTesting(
                              [&](base::expected<FlushedKey, FlushError> key) {
                                flushed = true;
                                EXPECT_TRUE(key.has_value());
                                EXPECT_EQ(key->size, info.size);
                                EXPECT_EQ(key->path, new_path);
                                // A new file is created, the creation time must
                                // be different.
                                EXPECT_GE(key->creation_time,
                                          info.creation_time);
                                EXPECT_EQ(buffer->proto()->events_size(), 0);
                                EXPECT_EQ(
                                    buffer->resource_info().used_size_bytes,
                                    0ul);
                              }));
  Wait();
  EXPECT_TRUE(flushed);
}

// ArenaEventBuffer functions the same regardless of the path, so there is only
// one test for this.
TEST_F(ArenaEventBufferTest, ProfileUpdatePath) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  AddProfile();
  Wait();

  EXPECT_NE(GetPath(), buffer->proto().path());

  EXPECT_EQ(buffer->AddEvent(TestEvent(2)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(3)), Result::kOk);

  // Write to disk at the new path.
  buffer->proto().StartWriteForTesting();
  Wait();

  // Expect the flushed size to be the same as the serialized size of the
  // in-memory content.
  int64_t size = 0;
  EXPECT_TRUE(base::GetFileSize(buffer->proto().path(), &size));
  EXPECT_EQ(static_cast<uint64_t>(size), buffer->proto()->ByteSizeLong());
}

TEST_F(ArenaEventBufferTest, PreInitEvents) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(2)), Result::kOk);
  EXPECT_EQ(buffer->AddEvent(TestEvent(3)), Result::kOk);

  Wait();

  EXPECT_EQ(buffer->proto()->events_size(), 3);
}

}  // namespace metrics::structured
