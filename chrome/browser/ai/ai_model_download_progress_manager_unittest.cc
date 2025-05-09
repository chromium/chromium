// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_model_download_progress_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/ai_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_ai {

using component_updater::CrxUpdateItem;
using testing::_;
using update_client::ComponentState;

class AIModelDownloadProgressManagerTest : public testing::Test {
 public:
  AIModelDownloadProgressManagerTest() = default;
  ~AIModelDownloadProgressManagerTest() override = default;

 protected:
  // Send a download update.
  void SendUpdate(const AITestUtils::FakeComponent& component,
                  ComponentState state,
                  uint64_t downloaded_bytes) {
    component_update_service_.SendUpdate(
        component.CreateUpdateItem(state, downloaded_bytes));
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  AITestUtils::MockComponentUpdateService component_update_service_;

 private:
  void SetUp() override {
    EXPECT_CALL(component_update_service_, GetComponentIDs()).Times(1);
  }
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AIModelDownloadProgressManagerTest,
       ReporterIsDestroyedWhenRemoteIsDisconnected) {
  AIModelDownloadProgressManager manager;

  // Should start with no reporters.
  EXPECT_EQ(manager.GetNumberOfReporters(), 0);

  AITestUtils::FakeComponent component("component_id", 100);

  {
    // Adding an Observer, should create a reporter.
    AITestUtils::FakeMonitor monitor1;
    manager.AddObserver(&component_update_service_,
                        monitor1.BindNewPipeAndPassRemote(), {component.id()});
    EXPECT_EQ(manager.GetNumberOfReporters(), 1);

    {
      // Adding a second observer we'll result in component ids being called
      // again.
      EXPECT_CALL(component_update_service_, GetComponentIDs()).Times(1);

      // Adding an Observer, should create a reporter.
      AITestUtils::FakeMonitor monitor2;
      manager.AddObserver(&component_update_service_,
                          monitor2.BindNewPipeAndPassRemote(),
                          {component.id()});
      EXPECT_EQ(manager.GetNumberOfReporters(), 2);
    }
    // `manager` should have destroyed the `Reporter` associated with
    // `monitor2`.
    base::test::RunUntil([&]() { return manager.GetNumberOfReporters() == 1; });
  }
  // `manager` should have destroyed the `Reporter` associated with
  // `monitor1`.
  base::test::RunUntil([&]() { return manager.GetNumberOfReporters() == 0; });
}

TEST_F(AIModelDownloadProgressManagerTest, FirstUpdateIsReportedAsZero) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // No events should be fired until the first update.
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));

  // The first update should be reported as zero. And `total_bytes` should
  // always be `kNormalizedProgressMax` (0x10000).
  SendUpdate(component, ComponentState::kDownloading, 10);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest, ProgressIsNormalized) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Should receive the first update.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update should have its downloaded_bytes normalized.
  uint64_t downloaded_bytes = 15;
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(downloaded_bytes,
                                              component.total_bytes());

  SendUpdate(component, ComponentState::kDownloading, downloaded_bytes);
  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       AlreadyDownloadedBytesArentIncludedInProgress) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  int64_t already_downloaded_bytes = 10;

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Send the first update with the already downloaded bytes for `component`.
  SendUpdate(component, ComponentState::kDownloading, already_downloaded_bytes);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update shouldn't include any already downloaded bytes.
  uint64_t downloaded_bytes = already_downloaded_bytes + 5;
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(
          downloaded_bytes - already_downloaded_bytes,
          component.total_bytes() - already_downloaded_bytes);

  SendUpdate(component, ComponentState::kDownloading, downloaded_bytes);
  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component(
      "component_id", AIUtils::kNormalizedDownloadProgressMax * 5);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Should receive the zero update.
  SendUpdate(component, ComponentState::kDownloading, 10);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Sending less than the total bytes should not send the
  // `kNormalizedDownloadProgressMax`.
  SendUpdate(component, ComponentState::kDownloading,
             component.total_bytes() - 1);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax - 1,
                               AIUtils::kNormalizedDownloadProgressMax);

  // Sending the total bytes should send the `kNormalizedDownloadProgressMax`.
  SendUpdate(component, ComponentState::kDownloading, component.total_bytes());
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsTotalBytesForFirstUpdate) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component(
      "component_id", AIUtils::kNormalizedDownloadProgressMax * 5);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // If the first update has downloaded bytes equal to total bytes, then both
  // the the zero and max events should be fired.
  SendUpdate(component, ComponentState::kDownloading, component.total_bytes());
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForNonDownloadEvents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Doesn't receive any update for these event states.
  for (const auto state : {
           ComponentState::kNew,
           ComponentState::kChecking,
           ComponentState::kCanUpdate,
           ComponentState::kUpdated,
           ComponentState::kUpdateError,
           ComponentState::kRun,
           ComponentState::kLastStatus,
       }) {
    SendUpdate(component, state, 10);
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));
  }
}

TEST_F(AIModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForEventsWithNegativeDownloadedBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Doesn't receive an update when the downloaded bytes are negative.
  SendUpdate(component, ComponentState::kDownloading, -1);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForEventsWithNegativeTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", -1);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Doesn't receive an update when the total bytes are negative.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForComponentsNotObserving) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component_observed("component_id1", 100);
  AITestUtils::FakeComponent component_not_observed("component_id2", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component_observed.id()});

  // Doesn't receive any update for these event states.
  SendUpdate(component_not_observed, ComponentState::kDownloading, 10);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest,
       ReceiveZeroAndHundredPercentForNoComponents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {});
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest, OnlyReceivesUpdatesEvery50ms) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Should receive the first update.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  SendUpdate(component, ComponentState::kDownloading, 15);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should receive the this since it's been over 50ms since the last update.
  SendUpdate(component, ComponentState::kDownloading, 20);
  monitor.ExpectReceivedNormalizedUpdate(20, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  SendUpdate(component, ComponentState::kDownloading, 25);
}

TEST_F(AIModelDownloadProgressManagerTest, OnlyReceivesUpdatesForNewProgress) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  // Set its total to twice kNormalizedProgressMax so that there are two raw
  // download progresses that map to every normalized download progress.
  AITestUtils::FakeComponent component(
      "component_id", AIUtils::kNormalizedDownloadProgressMax * 2);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Should receive the first update as zero.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should be able to receive this progress event since we haven't seen it
  // before.
  SendUpdate(component, ComponentState::kDownloading, 10);
  monitor.ExpectReceivedNormalizedUpdate(10, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive this progress event since we've just seen it.
  SendUpdate(component, ComponentState::kDownloading, 10);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive this progress event since it normalizes to a
  // progress we've seen.
  CHECK_EQ(
      AIUtils::NormalizeModelDownloadProgress(10, component.total_bytes()),
      AIUtils::NormalizeModelDownloadProgress(11, component.total_bytes()));
  SendUpdate(component, ComponentState::kDownloading, 11);
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest, ShouldReceive100percent) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component("component_id", 100);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(), {component.id()});

  // Should receive the first update.
  SendUpdate(component, ComponentState::kDownloading, 10);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Should receive the second update since it's 100% even though 50ms haven't
  // elapsed.
  SendUpdate(component, ComponentState::kDownloading, component.total_bytes());
  monitor.ExpectReceivedNormalizedUpdate(component.total_bytes(),
                                         component.total_bytes());

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest,
       AllComponentsMustBeObservedBeforeSendingEvents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id()});

  // Shouldn't receive this updates since we haven't observed `component2` yet.
  SendUpdate(component1, ComponentState::kDownloading, 0);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));

  // Should receive this update since now we've seen both components.
  SendUpdate(component2, ComponentState::kDownloading, 10);
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  monitor.ExpectReceivedNormalizedUpdate(0, total_bytes);
}

TEST_F(AIModelDownloadProgressManagerTest,
       ProgressIsNormalizedAgainstTheSumOfTheComponentsTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id()});

  // Trigger the first event by sending updates for components 1 and 2.
  uint64_t component1_downloaded_bytes = 0;
  SendUpdate(component1, ComponentState::kDownloading,
             component1_downloaded_bytes);
  uint64_t component2_downloaded_bytes = 0;
  SendUpdate(component2, ComponentState::kDownloading,
             component2_downloaded_bytes);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  SendUpdate(component2, ComponentState::kDownloading,
             component2_downloaded_bytes);

  // Should receive an update of the sum of component1 and component2's
  // downloaded bytes normalized with the sum of their total_bytes
  uint64_t downloaded_bytes =
      component1_downloaded_bytes + component2_downloaded_bytes;
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(downloaded_bytes, total_bytes);

  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       AlreadyDownloadedBytesArentIncludedInProgressForMultipleComponents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);
  int64_t already_downloaded_bytes = 0;

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id()});

  // Send an update for component 1.
  uint64_t component1_downloaded_bytes = 5;
  already_downloaded_bytes += 5;
  SendUpdate(component1, ComponentState::kDownloading,
             component1_downloaded_bytes);

  // Send a second update for component 1. This increases the already downloaded
  // bytes that shouldn't be included in the progress.
  component1_downloaded_bytes += 5;
  already_downloaded_bytes += 5;
  SendUpdate(component1, ComponentState::kDownloading,
             component1_downloaded_bytes);

  // Send an update for component 2 triggering the zero event. This increases
  // the already downloaded bytes that shouldn't be included in the progress.
  uint64_t component2_downloaded_bytes = 10;
  already_downloaded_bytes += 10;
  SendUpdate(component2, ComponentState::kDownloading,
             component2_downloaded_bytes);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  SendUpdate(component2, ComponentState::kDownloading,
             component2_downloaded_bytes);

  // The progress we receive shouldn't include the `already_downloaded_bytes`.
  uint64_t downloaded_bytes =
      component1_downloaded_bytes + component2_downloaded_bytes;
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(
          downloaded_bytes - already_downloaded_bytes,
          total_bytes - already_downloaded_bytes);

  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

class AIModelDownloadProgressManagerHasPreviousDownloadsTest
    : public AIModelDownloadProgressManagerTest {
 public:
  AIModelDownloadProgressManagerHasPreviousDownloadsTest() = default;
  ~AIModelDownloadProgressManagerHasPreviousDownloadsTest() override = default;

 private:
  // Don't expect that GetComponentIDs will return an empty vector.
  void SetUp() override {}
};

TEST_F(AIModelDownloadProgressManagerHasPreviousDownloadsTest,
       AlreadyInstalledComponentsAreNotObserved) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);

  EXPECT_CALL(component_update_service_, GetComponentIDs())
      .WillOnce(testing::Return(std::vector<std::string>({component1.id()})));

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id()});

  // Should receive this despite not observing component 1 yet since component1
  // is already downloaded.
  SendUpdate(component2, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedNormalizedUpdate(0, component2.total_bytes());
}

TEST_F(AIModelDownloadProgressManagerHasPreviousDownloadsTest,
       ProgressIsNormalizedAgainstOnlyUninstalledComponents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);
  AITestUtils::FakeComponent component3("component_id3", 500);

  EXPECT_CALL(component_update_service_, GetComponentIDs())
      .WillOnce(testing::Return(std::vector<std::string>({component1.id()})));

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id(), component3.id()});

  // Fire the zero progress event by sending events for component 2 and 3.
  SendUpdate(component2, ComponentState::kDownloading, 0);
  SendUpdate(component3, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Progress should be normalized against only components 2 and 3 since 1 is
  // already installed.
  SendUpdate(component2, ComponentState::kDownloading, 10);
  uint64_t total_bytes = component2.total_bytes() + component3.total_bytes();
  monitor.ExpectReceivedNormalizedUpdate(10, total_bytes);
}

TEST_F(AIModelDownloadProgressManagerHasPreviousDownloadsTest,
       ReceiveZeroAndHundredPercentWhenEverythingIsInstalled) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent component1("component_id1", 100);
  AITestUtils::FakeComponent component2("component_id2", 1000);

  EXPECT_CALL(component_update_service_, GetComponentIDs())
      .WillOnce(testing::Return(
          std::vector<std::string>({component1.id(), component2.id()})));

  manager.AddObserver(&component_update_service_,
                      monitor.BindNewPipeAndPassRemote(),
                      {component1.id(), component2.id()});
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
