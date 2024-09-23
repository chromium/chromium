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
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

constexpr uint64_t kSizeMb = 1024;
constexpr uint64_t kSize = kSizeMb * 1e6;
constexpr char kModel[] = "fabulous";
constexpr uint32_t kVendorIdNvme = 25;
constexpr uint16_t kVendorIdEmmc = 0xA5;
constexpr uint16_t kVendorIdEmmcLegacy = 0x5050;
constexpr uint16_t kVendorIdUfs = 0x1337;
constexpr uint32_t kProductIdNvme = 17;
constexpr uint64_t kProductIdEmmc = 0x4D4E504D4E50;
constexpr uint64_t kProductIdUfs =
    3210611189;  // base::PersistentHash("fabulous") = 3210611189.
constexpr uint32_t kRevisionNvme = 92;
constexpr uint8_t kRevisionEmmc = 0x8;
constexpr uint64_t kFwVersionNvme = 0xA0EF1;
constexpr uint64_t kFwVersionEmmc = 0x1223344556677889;
constexpr uint64_t kFwVersionUfs = 0x32323032;
constexpr char kSubsystemNvme[] = "block:nvme:pcie";
constexpr char kSubsystemEmmc[] = "block:mmc";
constexpr char kSubsystemUfs[] = "block:scsi:scsi:scsi:pci";
constexpr auto kTypeNvme =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::TYPE_NVME;
constexpr auto kTypeEmmc =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::TYPE_EMMC;
constexpr auto kTypeUfs =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::TYPE_UFS;
constexpr auto kMojoPurpose =
    ash::cros_healthd::mojom::StorageDevicePurpose::kBootDevice;
constexpr auto kUmaPurpose =
    metrics::SystemProfileProto::Hardware::InternalStorageDevice::PURPOSE_BOOT;

}  // namespace

class CrosHealthdMetricsProviderTest : public testing::Test {
 public:
  CrosHealthdMetricsProviderTest() {
    ash::cros_healthd::FakeCrosHealthd::Initialize();
  }

  void SetFakeCrosHealthdData(
      ash::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr storage_info) {
    std::vector<ash::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> devs;
    devs.push_back(std::move(storage_info));
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
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(CrosHealthdMetricsProviderTest, EndToEndWithNvme) {
  ash::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info;
  storage_info.device_info =
      ash::cros_healthd::mojom::BlockDeviceInfo::NewNvmeDeviceInfo(
          ash::cros_healthd::mojom::NvmeDeviceInfo::New(
              kVendorIdNvme, kProductIdNvme, kRevisionNvme, kFwVersionNvme));
  storage_info.vendor_id =
      ash::cros_healthd::mojom::BlockDeviceVendor::NewNvmeSubsystemVendor(
          kVendorIdNvme);
  storage_info.product_id =
      ash::cros_healthd::mojom::BlockDeviceProduct::NewNvmeSubsystemDevice(
          kProductIdNvme);
  storage_info.revision =
      ash::cros_healthd::mojom::BlockDeviceRevision::NewNvmePcieRev(
          kRevisionNvme);
  storage_info.firmware_version =
      ash::cros_healthd::mojom::BlockDeviceFirmware::NewNvmeFirmwareRev(
          kFwVersionNvme);
  storage_info.size = kSize;
  storage_info.name = kModel;
  storage_info.type = kSubsystemNvme;
  storage_info.purpose = kMojoPurpose;
  SetFakeCrosHealthdData(storage_info.Clone());

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

  EXPECT_EQ(kVendorIdNvme, dev.vendor_id());
  EXPECT_EQ(kProductIdNvme, dev.product_id());
  EXPECT_EQ(kRevisionNvme, dev.revision());
  EXPECT_EQ(kFwVersionNvme, dev.firmware_version());
  EXPECT_EQ(kSizeMb, dev.size_mb());
  EXPECT_EQ(kModel, dev.model());
  EXPECT_EQ(kTypeNvme, dev.type());
  EXPECT_EQ(kUmaPurpose, dev.purpose());
}

TEST_F(CrosHealthdMetricsProviderTest, EndToEndWithEmmc) {
  ash::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info;
  storage_info.device_info =
      ash::cros_healthd::mojom::BlockDeviceInfo::NewEmmcDeviceInfo(
          ash::cros_healthd::mojom::EmmcDeviceInfo::New(
              kVendorIdEmmc, kProductIdEmmc, kRevisionEmmc, kFwVersionEmmc));
  storage_info.vendor_id =
      ash::cros_healthd::mojom::BlockDeviceVendor::NewEmmcOemid(
          kVendorIdEmmcLegacy);
  storage_info.product_id =
      ash::cros_healthd::mojom::BlockDeviceProduct::NewEmmcPnm(kProductIdEmmc);
  storage_info.revision =
      ash::cros_healthd::mojom::BlockDeviceRevision::NewEmmcPrv(kRevisionEmmc);
  storage_info.firmware_version =
      ash::cros_healthd::mojom::BlockDeviceFirmware::NewEmmcFwrev(
          kFwVersionEmmc);
  storage_info.size = kSize;
  storage_info.name = kModel;
  storage_info.type = kSubsystemEmmc;
  storage_info.purpose = kMojoPurpose;
  SetFakeCrosHealthdData(storage_info.Clone());

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

  EXPECT_EQ(kVendorIdEmmc, dev.vendor_id());
  EXPECT_EQ(kProductIdEmmc, dev.product_id());
  EXPECT_EQ(kRevisionEmmc, dev.revision());
  EXPECT_EQ(kFwVersionEmmc, dev.firmware_version());
  EXPECT_EQ(kSizeMb, dev.size_mb());
  EXPECT_EQ(kModel, dev.model());
  EXPECT_EQ(kTypeEmmc, dev.type());
  EXPECT_EQ(kUmaPurpose, dev.purpose());
}

TEST_F(CrosHealthdMetricsProviderTest, EndToEndWithUfs) {
  ash::cros_healthd::mojom::NonRemovableBlockDeviceInfo storage_info;
  storage_info.device_info =
      ash::cros_healthd::mojom::BlockDeviceInfo::NewUfsDeviceInfo(
          ash::cros_healthd::mojom::UfsDeviceInfo::New(kVendorIdUfs,
                                                       kFwVersionUfs));
  storage_info.vendor_id =
      ash::cros_healthd::mojom::BlockDeviceVendor::NewJedecManfid(kVendorIdUfs);
  storage_info.product_id =
      ash::cros_healthd::mojom::BlockDeviceProduct::NewOther(0);
  storage_info.revision =
      ash::cros_healthd::mojom::BlockDeviceRevision::NewOther(0);
  storage_info.firmware_version =
      ash::cros_healthd::mojom::BlockDeviceFirmware::NewUfsFwrev(kFwVersionUfs);
  storage_info.size = kSize;
  storage_info.name = kModel;
  storage_info.type = kSubsystemUfs;
  storage_info.purpose = kMojoPurpose;
  SetFakeCrosHealthdData(storage_info.Clone());

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

  EXPECT_EQ(kVendorIdUfs, dev.vendor_id());
  EXPECT_EQ(kProductIdUfs, dev.product_id());
  EXPECT_EQ(kFwVersionUfs, dev.firmware_version());
  EXPECT_EQ(kSizeMb, dev.size_mb());
  EXPECT_EQ(kModel, dev.model());
  EXPECT_EQ(kTypeUfs, dev.type());
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
