// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_crx_component.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/task/current_thread.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "chrome/browser/ai/ai_model_download_progress_manager.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/ai_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_ai {

using component_updater::CrxUpdateItem;
using testing::_;
using update_client::ComponentState;

class AICrxComponentTest : public testing::Test {
 public:
  AICrxComponentTest() = default;
  ~AICrxComponentTest() override = default;

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

  AITestUtils::FakeComponent& CreateComponent(std::string id,
                                              uint64_t total_bytes) {
    auto [iter, emplaced] = fake_components_.try_emplace(id, id, total_bytes);
    CHECK(emplaced);
    return iter->second;
  }

  AITestUtils::MockComponentUpdateService component_update_service_;

 private:
  void SetUp() override {
    EXPECT_CALL(component_update_service_, GetComponentDetails(_, _))
        .WillRepeatedly([&](const std::string& id, CrxUpdateItem* item) {
          auto iter = fake_components_.find(id);
          if (iter == fake_components_.end()) {
            return false;
          }

          *item = iter->second.CreateUpdateItem(
              update_client::ComponentState::kNew, 0);

          return true;
        });
  }

  std::map<std::string, AITestUtils::FakeComponent> fake_components_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AICrxComponentTest, DoesntReceiveUpdatesForNonDownloadEvents) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent& component = CreateComponent("component_id", 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      AICrxComponent::FromComponentIds(
                          &component_update_service_, {component.id()}));

  // Doesn't receive any update for these event states.
  for (const auto state : {
           ComponentState::kNew,
           ComponentState::kChecking,
           ComponentState::kCanUpdate,
           ComponentState::kUpdated,
           ComponentState::kUpdateError,
           ComponentState::kRun,
       }) {
    SendUpdate(component, state, 10);
    monitor.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));
  }
}

TEST_F(AICrxComponentTest,
       DoesntReceiveUpdatesForEventsWithNegativeDownloadedBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent& component = CreateComponent("component_id", 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      AICrxComponent::FromComponentIds(
                          &component_update_service_, {component.id()}));

  // Doesn't receive an update when the downloaded bytes are negative.
  SendUpdate(component, ComponentState::kDownloading, -1);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AICrxComponentTest,
       DoesntReceiveUpdatesForEventsWithNegativeTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent& component = CreateComponent("component_id", -1);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      AICrxComponent::FromComponentIds(
                          &component_update_service_, {component.id()}));

  // Doesn't receive an update when the total bytes are negative.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AICrxComponentTest, DoesntReceiveUpdatesForComponentsNotObserving) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent& component_observed =
      CreateComponent("component_id1", 100);
  AITestUtils::FakeComponent& component_not_observed =
      CreateComponent("component_id2", 100);

  manager.AddObserver(
      monitor.BindNewPipeAndPassRemote(),
      AICrxComponent::FromComponentIds(&component_update_service_,
                                       {component_observed.id()}));

  // Doesn't receive any update for these event states.
  SendUpdate(component_not_observed, ComponentState::kDownloading, 10);
  monitor.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(AICrxComponentTest, ObservesComponentsMidDownload) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor1;
  AITestUtils::FakeMonitor monitor2;
  AITestUtils::FakeComponent& component = CreateComponent("component_id", 100);

  // First, `monitor1` observes `component`.
  {
    manager.AddObserver(monitor1.BindNewPipeAndPassRemote(),
                        AICrxComponent::FromComponentIds(
                            &component_update_service_, {component.id()}));
  }

  // Only `monitor1` will receive this update since `monitor2` is not observing.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor1.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  monitor2.ExpectNoUpdate();

  // Now both `monitor1` and `monitor2` are observing `component`.
  {
    manager.AddObserver(monitor2.BindNewPipeAndPassRemote(),
                        AICrxComponent::FromComponentIds(
                            &component_update_service_, {component.id()}));
  }

  // Send the first update to for `monitor2` waiting more than 50ms so that both
  // monitors receive it.
  constexpr int64_t update1_for_monitor2 = 60;
  FastForwardBy(base::Milliseconds(51));
  SendUpdate(component, ComponentState::kDownloading, update1_for_monitor2);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure update_callback =
        base::BarrierClosure(2, run_loop.QuitClosure());

    // `monitor1` should still be normalized against the total bytes of the
    // component.
    monitor1.ExpectReceivedNormalizedUpdate(
        update1_for_monitor2, component.total_bytes(), update_callback);

    // This is `monitor2`'s first update so it should receive zero and be
    // normalized against the remaining bytes.
    monitor2.ExpectReceivedNormalizedUpdate(
        0, component.total_bytes() - update1_for_monitor2, update_callback);

    run_loop.Run();
  }

  // Send a second update to for `monitor2` waiting more than 50ms so that both
  // monitors receive it.
  constexpr int64_t update2_for_monitor2 = 75;
  FastForwardBy(base::Milliseconds(51));
  SendUpdate(component, ComponentState::kDownloading, update2_for_monitor2);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure update_callback =
        base::BarrierClosure(2, run_loop.QuitClosure());

    // `monitor1` should still be normalized against the total bytes of the
    // component.
    monitor1.ExpectReceivedNormalizedUpdate(
        update2_for_monitor2, component.total_bytes(), update_callback);

    // `monitor2` should still be normalized against the remaining bytes it
    // observed on its first update. The downloaded bytes should also not
    // include any bytes that were downloaded before `monitor2` started
    // observing.
    monitor2.ExpectReceivedNormalizedUpdate(
        update2_for_monitor2 - update1_for_monitor2,
        component.total_bytes() - update1_for_monitor2, update_callback);

    run_loop.Run();
  }
}

TEST_F(AICrxComponentTest, DownloadedBytesWontExceedTotalBytes) {
  AIModelDownloadProgressManager manager;
  AITestUtils::FakeMonitor monitor;
  AITestUtils::FakeComponent& component = CreateComponent("component_id", 100);

  manager.AddObserver(monitor.BindNewPipeAndPassRemote(),
                      AICrxComponent::FromComponentIds(
                          &component_update_service_, {component.id()}));

  // Send a zero, so that the `AIModelDownloadProgressManager` sends the first
  // update. This ensures that the already downloaded bytes is zero.
  SendUpdate(component, ComponentState::kDownloading, 0);
  monitor.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  FastForwardBy(base::Milliseconds(51));

  // Sending an update that exceeds the component's total bytes is clamped to
  // the component's total bytes.
  SendUpdate(component, ComponentState::kDownloading,
             component.total_bytes() * 2);
  monitor.ExpectReceivedNormalizedUpdate(component.total_bytes(),
                                         component.total_bytes());
  FastForwardBy(base::Milliseconds(51));
}

}  // namespace on_device_ai
