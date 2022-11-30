// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/memory_pressure/arc_memory_pressure_bridge.h"

#include <stdint.h>
#include <unistd.h>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_process_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/session_manager/core/session_manager.h"

namespace arc {
namespace {

class AppKillObserver : public ArcMetricsService::AppKillObserver {
 public:
  AppKillObserver() = default;
  ~AppKillObserver() override = default;
  AppKillObserver(const AppKillObserver&) = delete;
  AppKillObserver& operator=(const AppKillObserver&) = delete;

  void OnArcLowMemoryKill() override {}
  void OnArcOOMKillCount(unsigned long count) override {}
  void OnArcMemoryPressureKill(int count, int estimated_freed_kb) override {
    arc_memory_pressure_kill_count_ = count;
    arc_memory_pressure_kill_estimated_freed_kb_ = estimated_freed_kb;
  }

  bool CheckLastMemoryPressureKill(int count, int estimated_freed_kb) const {
    return count == arc_memory_pressure_kill_count_ &&
           estimated_freed_kb == arc_memory_pressure_kill_estimated_freed_kb_;
  }

 private:
  int arc_memory_pressure_kill_count_ = -1;
  int arc_memory_pressure_kill_estimated_freed_kb_ = -1;
};

class ArcMemoryPressureBridgeTest : public testing::Test {
 protected:
  ArcMemoryPressureBridgeTest()
      : bridge_(GetBridge(context_, local_state_, kill_observer_)) {
    ArcServiceManager::Get()->arc_bridge_service()->process()->SetInstance(
        &fake_process_instance_);
  }
  ArcMemoryPressureBridgeTest(const ArcMemoryPressureBridgeTest&) = delete;
  ArcMemoryPressureBridgeTest& operator=(const ArcMemoryPressureBridgeTest&) =
      delete;

  ~ArcMemoryPressureBridgeTest() override {
    ash::SessionManagerClient::Shutdown();
    StabilityMetricsManager::Shutdown();
  }

  ArcMemoryPressureBridge* bridge() { return bridge_; }

  ash::FakeResourcedClient& resourced() { return fake_resourced_client_; }

  FakeProcessInstance& process_instance() { return fake_process_instance_; }

  const AppKillObserver& kill_observer() { return kill_observer_; }

 private:
  static ArcMemoryPressureBridge* GetBridge(
      TestBrowserContext& context,
      TestingPrefServiceSimple& local_state,
      AppKillObserver& kill_observer) {
    prefs::RegisterLocalStatePrefs(local_state.registry());
    StabilityMetricsManager::Initialize(&local_state);
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
    prefs::RegisterProfilePrefs(context.pref_registry());
    ArcMetricsService* const arc_metrics_service =
        ArcMetricsService::GetForBrowserContextForTesting(&context);
    DCHECK(arc_metrics_service != nullptr);
    arc_metrics_service->AddAppKillObserver(&kill_observer);
    return ArcMemoryPressureBridge::GetForBrowserContextForTesting(&context);
  }

  AppKillObserver kill_observer_;
  ash::FakeResourcedClient fake_resourced_client_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  session_manager::SessionManager session_manager_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  FakeProcessInstance fake_process_instance_;
  ArcMemoryPressureBridge* const bridge_;
};

TEST_F(ArcMemoryPressureBridgeTest, ConstructDestruct) {}

// Tests that NONE memory pressure does not trigger any kills.
TEST_F(ArcMemoryPressureBridgeTest, PressureNone) {
  ASSERT_NE(nullptr, bridge());
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::NONE,
      1 /* reclaim_target_kb */);
  ASSERT_TRUE(process_instance().IsLastHostMemoryPressureChecked());
}

// Tests that CACHED memory pressure triggers kills of R_CACHED_ACTIVITY
// priority.
TEST_F(ArcMemoryPressureBridgeTest, PressureCached) {
  ASSERT_NE(nullptr, bridge());
  // Check for overflow for large reclaim values by passing 5 GiB to the
  // callback, and then check that we report 5 MiB * KiB estimated_freed_kib.
  process_instance().set_apply_host_memory_pressure_response(
      1 /* killed */, UINT64_C(5368709120) /* reclaimed */);
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::CACHED,
      1 /* reclaim_target_kb */);
  ASSERT_TRUE(process_instance().CheckLastHostMemoryPressure(
      mojom::PressureLevel::kCached, 1024 /* reclaim_target */));

  // Run the ApplyHostMemoryPressure callback.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(kill_observer().CheckLastMemoryPressureKill(
      1 /* count */, 5242880 /* estimated_freed_kb */));
}

// Tests that PERCEPTIBLE memory pressure triggers kills of R_TOP priority.
TEST_F(ArcMemoryPressureBridgeTest, PressurePerceptible) {
  ASSERT_NE(nullptr, bridge());
  process_instance().set_apply_host_memory_pressure_response(
      1 /* killed */, 2048 /* reclaimed */);
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE,
      1 /* reclaim_target_kb */);
  ASSERT_TRUE(process_instance().CheckLastHostMemoryPressure(
      mojom::PressureLevel::kPerceptible, 1024 /* reclaim_target */));

  // Run the ApplyHostMemoryPressure callback.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(kill_observer().CheckLastMemoryPressureKill(
      1 /* count */, 2 /* estimated_freed_kb */));
}

// Tests that FOREGROUND memory pressure triggers kills of R_TOP priority.
TEST_F(ArcMemoryPressureBridgeTest, PressureForeground) {
  ASSERT_NE(nullptr, bridge());
  process_instance().set_apply_host_memory_pressure_response(
      1 /* killed */, 2048 /* reclaimed */);
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::FOREGROUND,
      1 /* reclaim_target_kb */);
  ASSERT_TRUE(process_instance().CheckLastHostMemoryPressure(
      mojom::PressureLevel::kForeground, 1024 /* reclaim_target */));

  // Run the ApplyHostMemoryPressure callback.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(kill_observer().CheckLastMemoryPressureKill(
      1 /* count */, 2 /* estimated_freed_kb */));
}

// Tests that multiple ArcVmMemoryPressure signals before the first completes
// only cause one call into ARCVM.
TEST_F(ArcMemoryPressureBridgeTest, DebouncePressure) {
  ASSERT_NE(nullptr, bridge());
  process_instance().set_apply_host_memory_pressure_response(
      1 /* killed */, 2048 /* reclaimed */);

  // FakeProcessInstance::HostMemoryPressure will DCHECK if
  // FakeProcessInstance::CheckLastHostMemoryPressure is not called first. So
  // these two calls in a row will fail if they are both forwarded to Mojo.
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE,
      1 /* reclaim_target_kb */);
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE,
      2 /* reclaim_target_kb */);

  // Check that the first call is the most recent one, meaning the second did
  // did not get forwarded to Mojo.
  ASSERT_TRUE(process_instance().CheckLastHostMemoryPressure(
      mojom::PressureLevel::kPerceptible, 1024 /* reclaim_target */));

  // Run the ApplyHostMemoryPressure callback.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(kill_observer().CheckLastMemoryPressureKill(
      1 /* count */, 2 /* estimated_freed_kb */));

  // Check that we do forward the next call after the callback is executed.
  process_instance().set_apply_host_memory_pressure_response(
      3 /* killed */, 4096 /* reclaimed */);
  resourced().FakeArcVmMemoryPressure(
      ash::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE,
      3 /* reclaim_target_kb */);
  ASSERT_TRUE(process_instance().CheckLastHostMemoryPressure(
      mojom::PressureLevel::kPerceptible, 3072 /* reclaim_target */));

  // Run the ApplyHostMemoryPressure callback.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(kill_observer().CheckLastMemoryPressureKill(
      3 /* count */, 4 /* estimated_freed_kb */));
}

}  // namespace
}  // namespace arc
