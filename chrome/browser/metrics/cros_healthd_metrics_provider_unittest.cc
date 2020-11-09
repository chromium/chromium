// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cros_healthd_metrics_provider.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {
constexpr uint32_t kVendorId = 25;
constexpr uint32_t kProductId = 17;
constexpr uint32_t kRevision = 92;
constexpr uint64_t kFwVersion = 0xA0EF1;
constexpr uint64_t kSizeMb = 1024;
constexpr uint64_t kSize = kSizeMb * 1e6;
constexpr char kModel[] = "fabulous";
constexpr char kSubsystem[] = "block:nvme:pcie";
constexpr auto kType =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::TYPE_NVME;
constexpr auto kMojoPurpose =
    chromeos::cros_healthd::mojom::StorageDevicePurpose::kSwapDevice;
constexpr auto kUmaPurpose =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::PURPOSE_SWAP;
}  // namespace

class CrosHealthdMetricsProviderTest : public testing::Test {
 public:
  CrosHealthdMetricsProviderTest() {
    chromeos::CrosHealthdClient::InitializeFake();

    chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info;
    storage_info.vendor_id = chromeos::cros_healthd::mojom::BlockDeviceVendor::
        NewNvmeSubsystemVendor(kVendorId);
    storage_info.product_id = chromeos::cros_healthd::mojom::
        BlockDeviceProduct::NewNvmeSubsystemDevice(kProductId);
    storage_info.revision =
        chromeos::cros_healthd::mojom::BlockDeviceRevision::NewNvmePcieRev(
            kRevision);
    storage_info.firmware_version =
        chromeos::cros_healthd::mojom::BlockDeviceFirmware::NewNvmeFirmwareRev(
            kFwVersion);
    storage_info.size = kSize;
    storage_info.name = kModel;
    storage_info.type = kSubsystem;
    storage_info.purpose = kMojoPurpose;

    std::vector<chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>
        devs;
    devs.push_back(storage_info.Clone());
    auto info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
    info->block_device_result = chromeos::cros_healthd::mojom::
        NonRemovableBlockDeviceResult::NewBlockDeviceInfo(std::move(devs));
    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  ~CrosHealthdMetricsProviderTest() override {
    chromeos::CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CrosHealthdMetricsProviderTest, EndToEnd) {
  base::RunLoop run_loop;
  CrosHealthdMetricsProvider provider;
  provider.AsyncInit(base::BindOnce(
      [](base::OnceClosure callback) { std::move(callback).Run(); },
      run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(provider.IsInitialized());
  metrics::SystemProfileProto profile;
  provider.ProvideSystemProfileMetrics(&profile);

  const auto& hardware = profile.hardware();
  ASSERT_EQ(1, hardware.internal_storage_devices_size());

  const auto& dev = hardware.internal_storage_devices(0);

  EXPECT_EQ(kVendorId, dev.vendor_id());
  EXPECT_EQ(kProductId, dev.product_id());
  EXPECT_EQ(kRevision, dev.revision());
  EXPECT_EQ(kFwVersion, dev.firmware_version());
  EXPECT_EQ(kSizeMb, dev.size_mb());
  EXPECT_EQ(kModel, dev.model());
  EXPECT_EQ(kType, dev.type());
  EXPECT_EQ(kUmaPurpose, dev.purpose());
}

TEST_F(CrosHealthdMetricsProviderTest, EndToEndTimeout) {
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()->SetCallbackDelay(
      CrosHealthdMetricsProvider::GetTimeout() +
      base::TimeDelta::FromSeconds(5));

  base::RunLoop run_loop;
  CrosHealthdMetricsProvider provider;
  provider.AsyncInit(base::BindOnce(
      [](base::OnceClosure callback) { std::move(callback).Run(); },
      run_loop.QuitClosure()));

  // FastForward by timeout period.
  task_environment_.FastForwardBy(CrosHealthdMetricsProvider::GetTimeout());
  run_loop.Run();

  EXPECT_FALSE(provider.IsInitialized());

  metrics::SystemProfileProto profile;
  provider.ProvideSystemProfileMetrics(&profile);

  const auto& hardware = profile.hardware();
  EXPECT_EQ(0, hardware.internal_storage_devices_size());
}
