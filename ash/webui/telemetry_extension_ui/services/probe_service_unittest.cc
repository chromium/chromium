// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/probe_service.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
namespace cros_healthd = ::ash::cros_healthd;
}  // namespace

class ProbeServiceTest : public testing::Test {
 public:
  void SetUp() override {
    auto fake_debugd_client = std::make_unique<FakeDebugDaemonClient>();
    fake_debugd_client_ = fake_debugd_client.get();

    chromeos::DBusThreadManager::Initialize();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        std::move(fake_debugd_client));

    cros_healthd::FakeCrosHealthd::Initialize();
  }

  void TearDown() override {
    cros_healthd::FakeCrosHealthd::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  health::mojom::ProbeServiceProxy* probe_service() const {
    return remote_probe_service_.get();
  }

  FakeDebugDaemonClient* fake_debugd_client() const {
    return fake_debugd_client_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<health::mojom::ProbeService> remote_probe_service_;
  ProbeService probe_service_{
      remote_probe_service_.BindNewPipeAndPassReceiver()};

  FakeDebugDaemonClient* fake_debugd_client_ = nullptr;
};

// Tests that ProbeTelemetryInfo requests telemetry info in cros_healthd and
// forwards response via callback.
TEST_F(ProbeServiceTest, ProbeTelemetryInfoSuccess) {
  constexpr int64_t kCycleCount = 512;

  {
    auto battery_info = cros_healthd::mojom::BatteryInfo::New();
    battery_info->cycle_count = kCycleCount;

    auto info = cros_healthd::mojom::TelemetryInfo::New();
    info->battery_result = cros_healthd::mojom::BatteryResult::NewBatteryInfo(
        std::move(battery_info));

    cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  base::RunLoop run_loop;
  probe_service()->ProbeTelemetryInfo(
      {health::mojom::ProbeCategoryEnum::kBattery},
      base::BindLambdaForTesting([&](health::mojom::TelemetryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        ASSERT_TRUE(ptr->battery_result);
        ASSERT_TRUE(ptr->battery_result->is_battery_info());
        ASSERT_TRUE(ptr->battery_result->get_battery_info());
        ASSERT_TRUE(ptr->battery_result->get_battery_info()->cycle_count);
        EXPECT_EQ(ptr->battery_result->get_battery_info()->cycle_count->value,
                  kCycleCount);

        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that GetOemData requests OEM data in debugd and
// forwards response via callback.
TEST_F(ProbeServiceTest, GetOemDataSuccess) {
  base::RunLoop run_loop;
  probe_service()->GetOemData(
      base::BindLambdaForTesting([&](health::mojom::OemDataPtr ptr) {
        ASSERT_TRUE(ptr);
        ASSERT_TRUE(ptr->oem_data.has_value());
        EXPECT_EQ(ptr->oem_data.value(), "oemdata: response from GetLog");

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace ash
