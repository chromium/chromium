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
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
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

class AshEventStorageTest : public testing::Test {
 public:
  AshEventStorageTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  AshEventStorageTest(const AshEventStorageTest&) = delete;
  AshEventStorageTest& operator=(const AshEventStorageTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetTestDirectory() { return temp_dir_.GetPath(); }
  base::FilePath GetProfilePath() { return profile_manager_.profiles_dir(); }
  base::FilePath GetUserDirectory() {
    return GetProfilePath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("user"));
  }

  std::unique_ptr<AshEventStorage> BuildTestStorage() {
    auto storage = std::make_unique<AshEventStorage>(
        /*write_delay=*/base::Seconds(0),
        GetTestDirectory()
            .Append(FILE_PATH_LITERAL("structured_metrics"))
            .Append(FILE_PATH_LITERAL("events")));
    // Wait for the device events to be loaded.
    Wait();
    return storage;
  }

  TestingProfile* AddProfile() {
    return profile_manager_.CreateTestingProfile("p1");
  }

  StructuredDataProto GetReport(AshEventStorage* storage) {
    StructuredDataProto structured_data;

    *structured_data.mutable_events() = storage->TakeEvents();

    return structured_data;
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;

 protected:
  TestingProfileManager profile_manager_;
};

TEST_F(AshEventStorageTest, StoreAndProvideEvents) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  AddProfile();

  Wait();

  ASSERT_TRUE(storage->IsReady());

  storage->AddEvent(BuildTestEvent());

  EventsProto events;
  storage->CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 1);

  StructuredDataProto proto = GetReport(storage.get());
  EXPECT_EQ(proto.events_size(), 1);

  // Storage should have no events after a successful dump.
  events.Clear();
  storage->CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 0);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, PreRecordedEventsProcessedCorrectly) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  storage->AddEvent(BuildTestEvent());
  // Wait for the device storage to be ready so this functions in the correct
  // order. If this isn't here there is a chance that the OnProfileAdded
  // finishes, calling AshEventStorage::OnProfileReady, before device storage
  // can loaded. This isn't an issue on device because there is likely to be a
  // long enough delay between the storage being created and the user
  // logging-in.
  Wait();

  // Add Profile and wait for the storage to be ready.
  AddProfile();
  Wait();

  ASSERT_TRUE(storage->IsReady());

  EventsProto events;
  storage->CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 1);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, EventsClearedAfterReport) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  AddProfile();

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
  TestingProfile* profile = AddProfile();
  Wait();

  storage->AddEvent(BuildTestEvent(0, {1234}));

  // Write events to disk and destroy the storage.
  storage.reset();
  Wait();

  storage = BuildTestStorage();

  // Ideally, this would test signing in to the same profile, but it's not clear
  // how to set that up, so instead we just call ProfileAdded() manually.
  storage->ProfileAdded(*profile);
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
  storage->CopyEvents(&events);
  EXPECT_EQ(events.events_size(), 1);
  ExpectNoErrors();

  AddProfile();
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

  AddProfile();
  Wait();

  EventsProto proto;
  *proto.add_events() = BuildTestEvent();
  *proto.add_events() = BuildTestEvent();
  *proto.add_events() = BuildTestEvent();
  storage->AddBatchEvents(proto.events());

  const auto data = GetReport(storage.get());
  ASSERT_EQ(data.events_size(), 3);

  ExpectNoErrors();
}

TEST_F(AshEventStorageTest, MergePreUserAndUserEvents) {
  std::unique_ptr<AshEventStorage> storage = BuildTestStorage();
  Wait();

  // Add event before OnProfileAdded is called.
  storage->AddEvent(BuildTestEvent());
  storage->AddEvent(BuildTestEvent());
  storage->AddEvent(BuildTestEvent());
  ASSERT_TRUE(storage->IsReady());

  // There should be 3 events in the pre-profile storage.
  EventsProto events_proto;
  storage->CopyEvents(&events_proto);
  EXPECT_EQ(events_proto.events_size(), 3);

  // Add profile and add an event while the profile events are being loaded.
  AddProfile();
  storage->AddEvent(BuildTestEvent());
  Wait();

  storage->AddEvent(BuildTestEvent());

  const auto data = GetReport(storage.get());
  EXPECT_EQ(data.events_size(), 5);

  ExpectNoErrors();
}

}  // namespace metrics::structured
