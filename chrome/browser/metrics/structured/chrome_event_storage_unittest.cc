// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_event_storage.h"

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
}  // namespace

class ChromeEventStorageTest : public testing::Test {
 public:
  void Wait() { task_environment_.RunUntilIdle(); }

  StructuredDataProto GetReport(ChromeEventStorage* storage) {
    StructuredDataProto structured_data;

    *structured_data.mutable_events() = storage->TakeEvents();

    return structured_data;
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

TEST_F(ChromeEventStorageTest, StoreAndProvideEvents) {
  ChromeEventStorage storage;

  Wait();

  ASSERT_TRUE(storage.IsReady());

  storage.AddEvent(BuildTestEvent());
  EXPECT_EQ(storage.RecordedEventsCount(), 1);

  EventsProto events;
  storage.CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 1);

  StructuredDataProto proto = GetReport(&storage);
  EXPECT_EQ(proto.events_size(), 1);

  // Storage should have no events after a successful dump.
  events.Clear();
  storage.CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 0);

  ExpectNoErrors();
}

TEST_F(ChromeEventStorageTest, EventsClearedAfterReport) {
  ChromeEventStorage storage;
  Wait();

  storage.AddEvent(BuildTestEvent());
  storage.AddEvent(BuildTestEvent());

  // Should provide both the previous events.
  EXPECT_EQ(GetReport(&storage).events_size(), 2);

  // But the previous events shouldn't appear in the second report.
  EXPECT_EQ(GetReport(&storage).events_size(), 0);

  storage.AddEvent(BuildTestEvent());
  // The third request should only contain the third event.
  EXPECT_EQ(GetReport(&storage).events_size(), 1);

  ExpectNoErrors();
}

TEST_F(ChromeEventStorageTest, Purge) {
  ChromeEventStorage storage;

  storage.AddEvent(BuildTestEvent());
  storage.AddEvent(BuildTestEvent());

  EXPECT_EQ(storage.RecordedEventsCount(), 2);

  storage.Purge();

  EXPECT_EQ(storage.RecordedEventsCount(), 0);
  EXPECT_EQ(GetReport(&storage).events_size(), 0);
}

}  // namespace metrics::structured
