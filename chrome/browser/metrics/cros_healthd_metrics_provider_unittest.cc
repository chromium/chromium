// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cros_healthd_metrics_provider.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
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
    ash::cros_healthd::mojom::StorageDevicePurpose::kSwapDevice;
constexpr auto kUmaPurpose =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::PURPOSE_SWAP;

}  // namespace

class CrosHealthdMetricsProviderTest : public testing::Test {
 public:
  CrosHealthdMetricsProviderTest() {
    ash::cros_healthd::FakeCrosHealthd::Initialize();

    ash::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info;
    storage_info.vendor_id =
        ash::cros_healthd::mojom::BlockDeviceVendor::NewNvmeSubsystemVendor(
            kVendorId);
    storage_info.product_id =
        ash::cros_healthd::mojom::BlockDeviceProduct::NewNvmeSubsystemDevice(
            kProductId);
    storage_info.revision =
        ash::cros_healthd::mojom::BlockDeviceRevision::NewNvmePcieRev(
            kRevision);
    storage_info.firmware_version =
        ash::cros_healthd::mojom::BlockDeviceFirmware::NewNvmeFirmwareRev(
            kFwVersion);
    storage_info.size = kSize;
    storage_info.name = kModel;
    storage_info.type = kSubsystem;
    storage_info.purpose = kMojoPurpose;

    std::vector<ash::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> devs;
    devs.push_back(storage_info.Clone());
    auto info = ash::cros_healthd::mojom::TelemetryInfo::New();
    info->block_device_result = ash::cros_healthd::mojom::
        NonRemovableBlockDeviceResult::NewBlockDeviceInfo(std::move(devs));
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  ~CrosHealthdMetricsProviderTest() override {
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
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
  ash::cros_healthd::FakeCrosHealthd::Get()->SetCallbackDelay(
      CrosHealthdMetricsProvider::GetTimeout() + base::Seconds(5));

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
