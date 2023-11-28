// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_event_storage.h"

#include <cstdint>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {
StructuredEventProto BuildTestEvent(
    uint64_t id = 0,
    const std::vector<int64_t>& metrics = std::vector<int64_t>()) {
  auto event = StructuredEventProto();
  event.set_device_project_id(id);
  int metric_id = 0;
  for (int64_t metric : metrics) {
    auto* m = event.add_metrics();
    m->set_name_hash(metric_id++);
    m->set_value_int64(metric);
  }
  return event;
}
}  // namespace

class AshEventStorageTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetTestDirectory() { return temp_dir_.GetPath(); }
  base::FilePath GetUserDirectory() {
    return temp_dir_.GetPath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("user"));
  }

  std::unique_ptr<AshEventStorage> BuildTestStorage() {
    return std::make_unique<AshEventStorage>(
        /*write_delay=*/base::Seconds(0),
        GetTestDirectory()
            .Append(FILE_PATH_LITERAL("structured_metrics"))
            .Append(FILE_PATH_LITERAL("events")));
  }

  StructuredDataProto GetReport(AshEventStorage* storage) {
    ChromeUserMetricsExtension uma;

    storage->MoveEvents(uma);

    return uma.structured_data();
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(AshEventStorageTest, StoreAndProvideEvents) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  storage->OnProfileAdded(GetUserDirectory());

  Wait();

  ASSERT_TRUE(storage->IsReady());

  storage->AddEvent(BuildTestEvent());

  EventsProto events;
  storage->GetEvents(&events);
  EXPECT_EQ(events.non_uma_events_size(), 1);

  StructuredDataProto proto = GetReport(storage.get());
  EXPECT_EQ(proto.events_size(), 1);

  // Storage should have no events after a successful dump.
  events.Clear();
  storage->GetEvents(&events);
  EXPECT_EQ(events.non_uma_events_size(), 0);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, PreRecordedEventsProcessedCorrectly) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  storage->AddEvent(BuildTestEvent());

  // Add Profile and wait for the storage to be ready.
  storage->OnProfileAdded(GetUserDirectory());
  Wait();

  ASSERT_TRUE(storage->IsReady());

  EventsProto events;
  storage->GetEvents(&events);
  EXPECT_EQ(events.non_uma_events_size(), 1);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, EventsClearedAfterReport) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  storage->OnProfileAdded(GetUserDirectory());

  Wait();

  storage->AddEvent(BuildTestEvent());
  storage->AddEvent(BuildTestEvent());

  // Should provide both the previous events.
  EXPECT_EQ(GetReport(storage.get()).events_size(), 2);

  // But the previous events shouldn't appear in the second report.
  EXPECT_EQ(GetReport(storage.get()).events_size(), 0);

  storage->AddEvent(BuildTestEvent());
  // The third request should only contain the third event.
  EXPECT_EQ(GetReport(storage.get()).events_size(), 1);

  ExpectNoErrors();
}

// Test that events recorded in one session are correctly persisted and are
// uploaded in the first report from a subsequent session.
TEST_F(AshEventStorageTest, EventsFromPreviousSessionAreLoaded) {
  // Start first session and record one event.
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  storage->OnProfileAdded(GetUserDirectory());
  Wait();

  storage->AddEvent(BuildTestEvent(0, {1234}));

  // Write events to disk and destroy the storage.
  storage.reset();
  Wait();

  storage = BuildTestStorage();
  storage->OnProfileAdded(GetUserDirectory());
  Wait();

  // Start a second session and ensure the event is reported.
  const auto data = GetReport(storage.get());
  ASSERT_EQ(data.events_size(), 1);
  ASSERT_EQ(data.events(0).metrics_size(), 1);
  EXPECT_EQ(data.events(0).metrics(0).value_int64(), 1234);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, EventsPreProfilePersistedCorrectly) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  Wait();

  // Add event before OnProfileAdded is called.
  storage->AddEvent(BuildTestEvent());
  ASSERT_TRUE(storage->IsReady());

  // Ensure that the event is persisted.
  EventsProto events;
  storage->GetEvents(&events);
  EXPECT_EQ(events.non_uma_events_size(), 1);
  ExpectNoErrors();

  storage->OnProfileAdded(GetUserDirectory());
  Wait();

  // Add another event OnProfileAdded is called.
  storage->AddEvent(BuildTestEvent());

  // Ensure that both events are in the report.
  const auto data = GetReport(storage.get());
  ASSERT_EQ(data.events_size(), 2);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, AddBatchEvents) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  Wait();

  storage->OnProfileAdded(GetUserDirectory());
  Wait();

  EventsProto proto;
  *proto.add_non_uma_events() = BuildTestEvent();
  *proto.add_non_uma_events() = BuildTestEvent();
  *proto.add_non_uma_events() = BuildTestEvent();
  storage->AddBatchEvents(proto.non_uma_events());

  const auto data = GetReport(storage.get());
  ASSERT_EQ(data.events_size(), 3);

  ExpectNoErrors();
}

}  // namespace metrics::structured
