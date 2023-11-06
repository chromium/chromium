// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_event_storage.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {
StructuredEventProto BuildTestEvent() {
  return StructuredEventProto();
}
}  // namespace

class AshEventStorageTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetUserDirectory() { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
};

TEST_F(AshEventStorageTest, StoreAndProvideEvents) {
  AshEventStorage storage(/*write_delay=*/base::Seconds(0));
  storage.OnProfileAdded(GetUserDirectory());

  Wait();

  ASSERT_TRUE(storage.IsReady());

  storage.AddEvent(BuildTestEvent());
  EXPECT_EQ(storage.events()->non_uma_events_size(), 1);

  ChromeUserMetricsExtension uma_proto;
  storage.MoveEvents(uma_proto);
  EXPECT_EQ(uma_proto.structured_data().events_size(), 1);
  EXPECT_EQ(storage.events()->non_uma_events_size(), 0);
}

TEST_F(AshEventStorageTest, PreRecordedEventsProcessedCorrectly) {
  AshEventStorage storage(/*write_delay=*/base::Seconds(0));

  storage.AddEvent(BuildTestEvent());

  // Add Profile and wait for the storage to be ready.
  storage.OnProfileAdded(GetUserDirectory());
  Wait();

  ASSERT_TRUE(storage.IsReady());

  // The proto should not be stored in the proto.
  EXPECT_EQ(storage.events()->non_uma_events_size(), 1);
}

}  // namespace metrics::structured
