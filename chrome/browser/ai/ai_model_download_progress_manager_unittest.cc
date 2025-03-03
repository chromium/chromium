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
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_ai {

using component_updater::CrxUpdateItem;
using testing::_;
using update_client::ComponentState;

namespace {

class FakeComponent {
 public:
  FakeComponent(std::string id, uint64_t total_bytes)
      : id_(std::move(id)), total_bytes_(total_bytes) {}

  CrxUpdateItem CreateUpdateItem(ComponentState state,
                                 uint64_t downloaded_bytes) const {
    CrxUpdateItem update_item;
    update_item.state = state;
    update_item.id = id_;
    update_item.downloaded_bytes = downloaded_bytes;
    update_item.total_bytes = total_bytes_;
    return update_item;
  }

  const std::string& id() { return id_; }
  uint64_t total_bytes() { return total_bytes_; }

 private:
  std::string id_;
  uint64_t total_bytes_;
};

class FakeMonitor {
 public:
  mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
  BindNewPipeAndPassRemote() {
    return mock_monitor_.BindNewPipeAndPassRemote();
  }

  void ExpectReceivedUpdate(uint64_t expected_downloaded_bytes,
                            uint64_t expected_total_bytes) {
    base::RunLoop download_progress_run_loop;
    EXPECT_CALL(mock_monitor_, OnDownloadProgressUpdate(_, _))
        .WillOnce(testing::Invoke(
            [&](uint64_t downloaded_bytes, uint64_t total_bytes) {
              EXPECT_EQ(downloaded_bytes, expected_downloaded_bytes);
              EXPECT_EQ(total_bytes, expected_total_bytes);
              download_progress_run_loop.Quit();
            }));
    download_progress_run_loop.Run();
  }

  void ExpectNoUpdate() {
    EXPECT_CALL(mock_monitor_, OnDownloadProgressUpdate(_, _)).Times(0);
  }

 private:
  AITestUtils::MockModelDownloadProgressMonitor mock_monitor_;
};

class AIModelProgressMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  AIModelProgressMockComponentUpdateService() = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void SendUpdate(const component_updater::CrxUpdateItem& item) {
    for (Observer& observer : observer_list_) {
      observer.OnEvent(item);
    }
  }

  // Not copyable or movable.
  AIModelProgressMockComponentUpdateService(
      const AIModelProgressMockComponentUpdateService&) = delete;
  AIModelProgressMockComponentUpdateService& operator=(
      const AIModelProgressMockComponentUpdateService&) = delete;

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace

class AIModelDownloadProgressManagerTest : public testing::Test {
 public:
  AIModelDownloadProgressManagerTest() = default;
  ~AIModelDownloadProgressManagerTest() override = default;

 protected:
  AIModelProgressMockComponentUpdateService component_update_service_;

  // Send a download update.
  void SendUpdate(const FakeComponent& monitor,
                  ComponentState state,
                  uint64_t downloaded_bytes) {
    component_update_service_.SendUpdate(
        monitor.CreateUpdateItem(state, downloaded_bytes));
  }

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
  EXPECT_EQ(manager.GetNumberOfReportersForTesting(), 0);

  FakeComponent component("component_id", 100);

  {
    // Adding an Observer, should create a reporter.
    FakeMonitor monitor1;
    manager.AddObserver(&component_update_service_,
                        monitor1.BindNewPipeAndPassRemote(), {component.id()});
    EXPECT_EQ(manager.GetNumberOfReportersForTesting(), 1);

    {
      // Adding an Observer, should create a reporter.
      FakeMonitor monitor2;
      manager.AddObserver(&component_update_service_,
                          monitor2.BindNewPipeAndPassRemote(),
                          {component.id()});
      EXPECT_EQ(manager.GetNumberOfReportersForTesting(), 2);
    }
    // `manager` should have destroyed the `Reporter` associated with
    // `monitor2`.
    base::test::RunUntil(
        [&]() { return manager.GetNumberOfReportersForTesting() == 1; });
  }
  // `manager` should have destroyed the `Reporter` associated with
  // `monitor1`.
  base::test::RunUntil(
      [&]() { return manager.GetNumberOfReportersForTesting() == 0; });
}

TEST_F(AIModelDownloadProgressManagerTest, FirstUpdateIsReportedAsZero) {
  AIModelDownloadProgressManager manager;
  FakeMonitor monitor;
  FakeComponent component("component_id", 100);

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

}  // namespace on_device_ai
