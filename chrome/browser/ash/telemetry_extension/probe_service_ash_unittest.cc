// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/probe_service_ash.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ProbeServieAshTest : public testing::Test {
 public:
  void SetUp() override {
    DebugDaemonClient::InitializeFake();
    cros_healthd::FakeCrosHealthd::Initialize();
  }

  void TearDown() override {
    cros_healthd::FakeCrosHealthd::Shutdown();
    DebugDaemonClient::Shutdown();
  }

  crosapi::mojom::TelemetryProbeServiceProxy* probe_service() const {
    return remote_probe_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<crosapi::mojom::TelemetryProbeService> remote_probe_service_;
  std::unique_ptr<crosapi::mojom::TelemetryProbeService> probe_service_{
      ProbeServiceAsh::Factory::Create(
          remote_probe_service_.BindNewPipeAndPassReceiver())};
};

// Tests that ProbeTelemetryInfo requests telemetry info in cros_healthd and
// forwards response via callback.
TEST_F(ProbeServieAshTest, ProbeTelemetryInfoSuccess) {
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
      {crosapi::mojom::ProbeCategoryEnum::kBattery},
      base::BindLambdaForTesting(
          [&](crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
            ASSERT_TRUE(ptr);
            ASSERT_TRUE(ptr->battery_result);
            ASSERT_TRUE(ptr->battery_result->is_battery_info());
            ASSERT_TRUE(ptr->battery_result->get_battery_info());
            ASSERT_TRUE(ptr->battery_result->get_battery_info()->cycle_count);
            EXPECT_EQ(
                ptr->battery_result->get_battery_info()->cycle_count->value,
                kCycleCount);

            run_loop.Quit();
          }));
  run_loop.Run();
}

// Tests that GetOemData requests OEM data in debugd and
// forwards response via callback.
TEST_F(ProbeServieAshTest, GetOemDataSuccess) {
  base::RunLoop run_loop;
  probe_service()->GetOemData(
      base::BindLambdaForTesting([&](crosapi::mojom::ProbeOemDataPtr ptr) {
        ASSERT_TRUE(ptr);
        ASSERT_TRUE(ptr->oem_data.has_value());
        EXPECT_EQ(ptr->oem_data.value(), "oemdata: response from GetLog");

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace ash
