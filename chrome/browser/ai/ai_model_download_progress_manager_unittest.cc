// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_model_download_progress_manager.h"

#include <cstdint>
#include <memory>

#include "base/task/current_thread.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/ai_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_ai {

using testing::_;
using ComponentList =
    base::flat_set<std::unique_ptr<AIModelDownloadProgressManager::Component>>;

namespace {

class FakeComponent {
 public:
  FakeComponent(std::optional<int64_t> downloaded_bytes,
                std::optional<int64_t> total_bytes)
      : downloaded_bytes_(downloaded_bytes), total_bytes_(total_bytes) {}

  int64_t total_bytes() {
    CHECK(total_bytes_.has_value());
    return total_bytes_.value();
  }

  std::unique_ptr<AIModelDownloadProgressManager::Component> GetImpl() {
    CHECK(!impl_);

    std::unique_ptr<Impl> impl = std::make_unique<Impl>();
    impl_ = impl->weak_ptr_factory_.GetWeakPtr();

    // Update total bytes if its already been set.
    if (total_bytes_) {
      impl_->SetTotalBytes(total_bytes_.value());
    }
    // Update downloaded bytes if its already been set.
    if (downloaded_bytes_) {
      impl_->SetDownloadedBytes(downloaded_bytes_.value());
    }

    return impl;
  }

  ComponentList GetImplAsList() {
    ComponentList component_list;
    component_list.insert(GetImpl());
    return component_list;
  }

  void SetTotalBytes(int64_t total_bytes) {
    total_bytes_ = total_bytes;

    if (impl_) {
      impl_->SetTotalBytes(total_bytes);
    }
  }

  void SetDownloadedBytes(int64_t downloaded_bytes) {
    downloaded_bytes_ = downloaded_bytes;

    if (impl_) {
      impl_->SetDownloadedBytes(downloaded_bytes);
    }
  }

 private:
  class Impl : public AIModelDownloadProgressManager::Component {
   protected:
    friend FakeComponent;
    base::WeakPtrFactory<Impl> weak_ptr_factory_{this};
  };

  base::WeakPtr<Impl> impl_;

  std::optional<int64_t> downloaded_bytes_;
  std::optional<int64_t> total_bytes_;
};

}  // namespace

class AIModelDownloadProgressManagerTest : public testing::Test {
 public:
  AIModelDownloadProgressManagerTest() = default;
  ~AIModelDownloadProgressManagerTest() override = default;

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AIModelDownloadProgressManagerTest,
       ReporterIsDestroyedWhenRemoteIsDisconnected) {
  AIModelDownloadProgressManager manager;

  // Should start with no reporters.
  EXPECT_EQ(manager.GetNumberOfReporters(), 0);

  {
    // Adding an Observer, should create a reporter.
    FakeComponent component1(std::nullopt, std::nullopt);
    AITestUtils::FakeMonitor monitor1;
    manager.AddObserver(monitor1.BindNewPipeAndPassRemote(),
                        component1.GetImplAsList());
    EXPECT_EQ(manager.GetNumberOfReporters(), 1);

    {
      // Adding an Observer, should create a reporter.
      FakeComponent component2(std::nullopt, std::nullopt);
      AITestUtils::FakeMonitor monitor2;
      manager.AddObserver(monitor2.BindNewPipeAndPassRemote(),
                          component2.GetImplAsList());
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

TEST_F(AIModelDownloadProgressManagerTest,
       DoesntReceiveUpdateUntilAllBytesAreDetermined) {
  // Both total and download bytes are undetermined.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component(std::nullopt, std::nullopt);

    // No events should be fired since bytes haven't been determined yet.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        component.GetImplAsList());
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));

    // No events should be fired since total bytes hasn't been determined yet.
    component.SetDownloadedBytes(0);
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));

    // Bytes have been determined so we should receive the first update.
    component.SetTotalBytes(100);
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }

  // Download bytes are undetermined.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component(std::nullopt, 100);

    // No events should be fired since downloaded bytes hasn't been determined
    // yet.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        component.GetImplAsList());
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));

    // Bytes have been determined so we should receive the first update.
    component.SetDownloadedBytes(0);
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }

  // Total bytes are undetermined.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component(0, std::nullopt);

    // No events should be fired since total bytes hasn't been determined yet.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        component.GetImplAsList());
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));

    // Bytes have been determined so we should receive the first update.
    component.SetTotalBytes(100);
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }

  // Multiple components with one having undetermined downloaded bytes.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component1(0, 100);
    FakeComponent component2(std::nullopt, 1000);
    ComponentList component_list;
    component_list.insert(component1.GetImpl());
    component_list.insert(component2.GetImpl());

    // No events should be fired since `component2`'s downloaded bytes haven't
    // been determined yet.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        std::move(component_list));
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));

    // Bytes have been determined so we should receive the first update.
    component2.SetDownloadedBytes(0);
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }
}

TEST_F(AIModelDownloadProgressManagerTest,
       SendsUpdateIfBytesAreAlreadyDetermined) {
  // One component.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component(0, 100);

    // We should get the first update since all bytes have been determined.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        component.GetImplAsList());
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }

  // Two components.
  {
    AIModelDownloadProgressManager manager;
    AITestUtils::FakeMonitor monitor;
    FakeComponent component1(0, 100);
    FakeComponent component2(0, 1000);
    ComponentList component_list;
    component_list.insert(component1.GetImpl());
    component_list.insert(component2.GetImpl());

    // We should get the first update since all bytes have been determined.
    manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                        std::move(component_list));
    monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  }
}

TEST_F(AIModelDownloadProgressManagerTest, FirstUpdateIsReportedAsZero) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(10, 100);

  // The first update should be reported as zero. And `total_bytes` should
  // always be `kNormalizedProgressMax` (0x10000).
  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest, ProgressIsNormalized) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt, 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Should receive the first update.
  component.SetDownloadedBytes(0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update should have its downloaded_bytes normalized.
  uint64_t downloaded_bytes = 15;
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(downloaded_bytes,
                                              component.total_bytes());

  component.SetDownloadedBytes(downloaded_bytes);
  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       AlreadyDownloadedBytesArentIncludedInProgress) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt, 100);

  int64_t already_downloaded_bytes = 10;

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Send the first update with the already downloaded bytes for `component`.
  component.SetDownloadedBytes(already_downloaded_bytes);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update shouldn't include any already downloaded bytes.
  uint64_t downloaded_bytes = already_downloaded_bytes + 5;
  uint64_t normalized_downloaded_bytes =
      AIUtils::NormalizeModelDownloadProgress(
          downloaded_bytes - already_downloaded_bytes,
          component.total_bytes() - already_downloaded_bytes);

  component.SetDownloadedBytes(downloaded_bytes);
  monitor.ExpectReceivedUpdate(normalized_downloaded_bytes,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt,
                          AIUtils::kNormalizedDownloadProgressMax * 5);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Should receive the zero update.
  component.SetDownloadedBytes(10);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Sending less than the total bytes should not send the
  // `kNormalizedDownloadProgressMax`.
  component.SetDownloadedBytes(component.total_bytes() - 1);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax - 1,
                               AIUtils::kNormalizedDownloadProgressMax);

  // Sending the total bytes should send the `kNormalizedDownloadProgressMax`.
  component.SetDownloadedBytes(component.total_bytes());
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsTotalBytesForFirstUpdate) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt,
                          AIUtils::kNormalizedDownloadProgressMax * 5);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // If the first update has downloaded bytes equal to total bytes, then both
  // the the zero and max events should be fired.
  component.SetDownloadedBytes(component.total_bytes());
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest,
       ReceiveZeroAndHundredPercentForNoComponents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(), {});
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

TEST_F(AIModelDownloadProgressManagerTest, OnlyReceivesUpdatesEvery50ms) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt, 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Should receive the first update.
  component.SetDownloadedBytes(0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  component.SetDownloadedBytes(15);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should receive the this since it's been over 50ms since the last update.
  component.SetDownloadedBytes(20);
  monitor.ExpectReceivedNormalizedUpdate(20, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  component.SetDownloadedBytes(25);
}

TEST_F(AIModelDownloadProgressManagerTest, OnlyReceivesUpdatesForNewProgress) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  // Set its total to twice kNormalizedProgressMax so that there are two raw
  // download progresses that map to every normalized download progress.
  FakeComponent component(std::nullopt,
                          AIUtils::kNormalizedDownloadProgressMax * 2);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Should receive the first update as zero.
  component.SetDownloadedBytes(0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should be able to receive this progress event since we haven't seen it
  // before.
  component.SetDownloadedBytes(10);
  monitor.ExpectReceivedNormalizedUpdate(10, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive these progress updates since they're not
  // greater than the last progress update.
  component.SetDownloadedBytes(10);
  component.SetDownloadedBytes(9);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive this progress event since it normalizes to a
  // progress we've seen.
  CHECK_EQ(
      AIUtils::NormalizeModelDownloadProgress(10, component.total_bytes()),
      AIUtils::NormalizeModelDownloadProgress(11, component.total_bytes()));
  component.SetDownloadedBytes(11);
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest, ShouldReceive100percent) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component(std::nullopt, 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      component.GetImplAsList());

  // Should receive the first update.
  component.SetDownloadedBytes(10);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Should receive the second update since it's 100% even though 50ms haven't
  // elapsed.
  component.SetDownloadedBytes(component.total_bytes());
  monitor.ExpectReceivedNormalizedUpdate(component.total_bytes(),
                                         component.total_bytes());

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AIModelDownloadProgressManagerTest,
       AllComponentsMustBeObservedBeforeSendingEvents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component1(std::nullopt, 100);
  FakeComponent component2(std::nullopt, 1000);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));

  // Shouldn't receive this updates since we haven't observed `component2` yet.
  component1.SetDownloadedBytes(0);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));

  // Should receive this update since now we've seen both components.
  component2.SetDownloadedBytes(10);
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  monitor.ExpectReceivedNormalizedUpdate(0, total_bytes);
}

TEST_F(AIModelDownloadProgressManagerTest,
       ProgressIsNormalizedAgainstTheSumOfTheComponentsTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component1(std::nullopt, 100);
  FakeComponent component2(std::nullopt, 1000);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));

  // Trigger the first event by sending updates for components 1 and 2.
  uint64_t component1_downloaded_bytes = 0;
  component1.SetDownloadedBytes(component1_downloaded_bytes);
  uint64_t component2_downloaded_bytes = 0;
  component2.SetDownloadedBytes(component2_downloaded_bytes);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  component2.SetDownloadedBytes(component2_downloaded_bytes);

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
  FakeComponent component1(std::nullopt, 100);
  FakeComponent component2(std::nullopt, 1000);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());
  int64_t already_downloaded_bytes = 0;

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));

  // Send an update for component 1.
  uint64_t component1_downloaded_bytes = 5;
  already_downloaded_bytes += 5;
  component1.SetDownloadedBytes(component1_downloaded_bytes);

  // Send a second update for component 1. This increases the already downloaded
  // bytes that shouldn't be included in the progress.
  component1_downloaded_bytes += 5;
  already_downloaded_bytes += 5;
  component1.SetDownloadedBytes(component1_downloaded_bytes);

  // Send an update for component 2 triggering the zero event. This increases
  // the already downloaded bytes that shouldn't be included in the progress.
  uint64_t component2_downloaded_bytes = 10;
  already_downloaded_bytes += 10;
  component2.SetDownloadedBytes(component2_downloaded_bytes);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  component2.SetDownloadedBytes(component2_downloaded_bytes);

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

TEST_F(AIModelDownloadProgressManagerTest,
       AlreadyInstalledComponentsAreNotObserved) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component1(100, 100);
  FakeComponent component2(std::nullopt, 1000);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));

  // Should receive this despite not observing component 1 yet since component1
  // is already downloaded.
  component2.SetDownloadedBytes(0);
  monitor.ExpectReceivedNormalizedUpdate(0, component2.total_bytes());
}

TEST_F(AIModelDownloadProgressManagerTest,
       ProgressIsNormalizedAgainstOnlyUninstalledComponents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component1(100, 100);
  FakeComponent component2(std::nullopt, 1000);
  FakeComponent component3(std::nullopt, 500);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());
  component_list.insert(component3.GetImpl());

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));

  // Fire the zero progress event by sending events for component 2 and 3.
  component2.SetDownloadedBytes(0);
  component3.SetDownloadedBytes(0);
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Progress should be normalized against only components 2 and 3 since 1 is
  // already installed.
  component2.SetDownloadedBytes(10);
  uint64_t total_bytes = component2.total_bytes() + component3.total_bytes();
  monitor.ExpectReceivedNormalizedUpdate(10, total_bytes);
}

TEST_F(AIModelDownloadProgressManagerTest,
       ReceiveZeroAndHundredPercentWhenEverythingIsInstalled) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  FakeComponent component1(100, 100);
  FakeComponent component2(1000, 1000);
  ComponentList component_list;
  component_list.insert(component1.GetImpl());
  component_list.insert(component2.GetImpl());

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      std::move(component_list));
  monitor.ExpectReceivedUpdate(0, AIUtils::kNormalizedDownloadProgressMax);
  monitor.ExpectReceivedUpdate(AIUtils::kNormalizedDownloadProgressMax,
                               AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
