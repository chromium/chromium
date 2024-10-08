// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_reven_hardware_checker.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace mojom = ash::cros_healthd::mojom;

namespace {

constexpr uint64_t kFakeUnusedIntValue = 0;
constexpr uint64_t kMemorySizeMeetsRequirementInKiB = 4ULL * 1024ULL * 1024ULL;
constexpr uint64_t kMemorySizeBelowRequirementInKiB = 2ULL * 1024ULL * 1024ULL;
constexpr uint64_t kStorageSizeMeetsRequirementInBytes =
    32ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t kStorageSizeBelowRequirementInBytes =
    16ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t kVendorId = 0x8086;
// ID for a supported GPU device.
constexpr uint64_t kAllowGpuDeviceId = 0x9a49;
// ID for a supported WiFi device.
constexpr uint64_t kAllowWiFiDeviceId = 0x095a;
constexpr uint64_t kNotAllowGpuDeviceId = 0x0001;
constexpr char kFakeUnusedStrValue[] = "fake_string";
constexpr bool kMeetHwRequirement = true;
constexpr bool kNotMeetHwRequirement = false;
constexpr bool kHasKvmDevice = true;
constexpr bool kNoKvmDevice = false;
constexpr bool kNotSpinHdd = false;
constexpr mojom::StorageDevicePurpose kStoragePurpose =
    mojom::StorageDevicePurpose::kBootDevice;

class ArcRevenHardwareCheckerTest : public testing::Test {
 public:
  ArcRevenHardwareCheckerTest() = default;
  ~ArcRevenHardwareCheckerTest() override = default;

  ArcRevenHardwareCheckerTest(const ArcRevenHardwareCheckerTest&) = delete;
  ArcRevenHardwareCheckerTest& operator=(const ArcRevenHardwareCheckerTest&) =
      delete;

  void SetUp() override {
    ash::cros_healthd::FakeCrosHealthd::InitializeInBrowserTest();
  }

  void TearDown() override {
    ash::cros_healthd::FakeCrosHealthd::ShutdownInBrowserTest();
  }

  void SetFakeTelemetryInfoResponse(mojom::TelemetryInfoPtr info) {
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  // Runs the hardware checker and expects a specific compatibility result
  void RunCheckerAndExpect(bool expected_compatibility) {
    base::RunLoop run_loop;
    bool result_received = false;

    auto callback = base::BindOnce(
        [](base::RunLoop* run_loop, bool* result_received, bool result) {
          *result_received = result;
          run_loop->Quit();
        },
        &run_loop, &result_received);

    checker_.IsRevenDeviceCompatibleForArc(std::move(callback));
    run_loop.Run();
    EXPECT_EQ(expected_compatibility, result_received);
  }

  mojom::MemoryResultPtr CreateMemoryResult(uint64_t memory_kib) {
    auto memory_info = mojom::MemoryInfo::New();
    memory_info->total_memory_kib = memory_kib;
    return mojom::MemoryResult::NewMemoryInfo({std::move(memory_info)});
  }

  mojom::CpuResultPtr CreateCpuResult(bool kvm_device) {
    auto cpu_info = mojom::CpuInfo::New();
    cpu_info->virtualization = mojom::VirtualizationInfo::New();
    cpu_info->virtualization->has_kvm_device = kvm_device;
    return mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
  }

  mojom::NonRemovableBlockDeviceResultPtr CreateBlockDeviceResult(
      uint64_t storage_size) {
    std::vector<mojom::NonRemovableBlockDeviceInfoPtr> storage_vector;
    storage_vector.push_back(mojom::NonRemovableBlockDeviceInfo::New(
        /*bytes_read=*/kFakeUnusedIntValue,
        /*bytes_written=*/kFakeUnusedIntValue,
        /*read_time=*/kFakeUnusedIntValue, /*write_time=*/kFakeUnusedIntValue,
        /*io_time=*/kFakeUnusedIntValue,
        /*discard_time=*/mojom::NullableUint64::New(kFakeUnusedIntValue),
        /*device_info=*/
        mojom::BlockDeviceInfo::NewEmmcDeviceInfo(mojom::EmmcDeviceInfo::New(
            /*manfid=*/kFakeUnusedIntValue, /*pnm=*/kFakeUnusedIntValue,
            /*prv=*/kFakeUnusedIntValue, /*fwrev=*/kFakeUnusedIntValue)),
        /*vendor=*/mojom::BlockDeviceVendor::NewEmmcOemid(kFakeUnusedIntValue),
        /*product=*/mojom::BlockDeviceProduct::NewEmmcPnm(kFakeUnusedIntValue),
        /*revision=*/
        mojom::BlockDeviceRevision::NewEmmcPrv(kFakeUnusedIntValue),
        /*name=*/kFakeUnusedStrValue,
        /*size=*/storage_size,
        /*firmware_version=*/
        mojom::BlockDeviceFirmware::NewEmmcFwrev(kFakeUnusedIntValue),
        /*type=*/kFakeUnusedStrValue, /*purpose=*/kStoragePurpose,
        /*path=*/kFakeUnusedStrValue, /*manufacturer=*/kFakeUnusedIntValue,
        /*serial=*/kFakeUnusedIntValue, /*firmware_string=*/kFakeUnusedStrValue,
        /*is_rotational=*/kNotSpinHdd));
    return mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
        std::move(storage_vector));
  }

  mojom::NonRemovableBlockDeviceResultPtr CreateErrorStorageTag() {
    auto error = ash::cros_healthd::mojom::ProbeError::New();
    return ash::cros_healthd::mojom::NonRemovableBlockDeviceResult::NewError(
        std::move(error));
  }

  mojom::BusResultPtr CreateBusResult(uint64_t gpu_device_id,
                                      uint64_t wifi_device_id) {
    std::vector<mojom::BusDevicePtr> bus_devices;
    auto wifi_device = CreateBusDevice(
        mojom::BusDeviceClass::kWirelessController, kVendorId, wifi_device_id);
    auto gpu_device = CreateBusDevice(mojom::BusDeviceClass::kDisplayController,
                                      kVendorId, gpu_device_id);
    bus_devices.push_back(std::move(wifi_device));
    bus_devices.push_back(std::move(gpu_device));
    return mojom::BusResult::NewBusDevices(std::move(bus_devices));
  }

  mojom::BusDevicePtr CreateBusDevice(mojom::BusDeviceClass dev_class,
                                      uint64_t vendor_id,
                                      uint64_t device_id) {
    // pci bus device
    auto pci_bus_info = mojom::PciBusInfo::New(
        /*class=*/kFakeUnusedIntValue,
        /*subclass=*/kFakeUnusedIntValue,
        /*prog=*/kFakeUnusedIntValue,
        /*vendor_id=*/vendor_id,
        /*device_id=*/device_id,
        /*driver=*/kFakeUnusedStrValue);
    return mojom::BusDevice::New(
        /*vendor_name=*/kFakeUnusedStrValue,
        /*product_name=*/kFakeUnusedStrValue,
        /*device=*/dev_class,
        mojom::BusInfo::NewPciBusInfo(std::move(pci_bus_info)));
  }

  ArcRevenHardwareChecker checker_;

 protected:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

// Test failure due to insufficient memory size.
TEST_F(ArcRevenHardwareCheckerTest, MemoryRequirementNotMet) {
  auto info = mojom::TelemetryInfo::New();
  info->block_device_result =
      CreateBlockDeviceResult(kStorageSizeMeetsRequirementInBytes);
  info->memory_result = CreateMemoryResult(kMemorySizeBelowRequirementInKiB);
  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(kNotMeetHwRequirement);
}

// Test failure due to absent KVM support.
TEST_F(ArcRevenHardwareCheckerTest, CpuRequirementNotMet) {
  auto info = mojom::TelemetryInfo::New();
  info->memory_result = CreateMemoryResult(kMemorySizeMeetsRequirementInKiB);
  info->block_device_result =
      CreateBlockDeviceResult(kStorageSizeMeetsRequirementInBytes);
  info->cpu_result = CreateCpuResult(kNoKvmDevice);
  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(kNotMeetHwRequirement);
}

// Test failure due to insufficient storage size.
TEST_F(ArcRevenHardwareCheckerTest, StorageRequirementNotMet) {
  auto info = mojom::TelemetryInfo::New();
  info->memory_result = CreateMemoryResult(kMemorySizeMeetsRequirementInKiB);
  info->cpu_result = CreateCpuResult(kHasKvmDevice);
  info->block_device_result =
      CreateBlockDeviceResult(kStorageSizeBelowRequirementInBytes);
  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(kNotMeetHwRequirement);
}

// Test failure due to unsupported GPU device ID.
TEST_F(ArcRevenHardwareCheckerTest, BusRequirementNotMet) {
  auto info = mojom::TelemetryInfo::New();
  info->memory_result = CreateMemoryResult(kMemorySizeMeetsRequirementInKiB);
  info->cpu_result = CreateCpuResult(kHasKvmDevice);
  info->block_device_result =
      CreateBlockDeviceResult(kStorageSizeMeetsRequirementInBytes);
  info->bus_result = CreateBusResult(kNotAllowGpuDeviceId, kAllowWiFiDeviceId);
  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(kNotMeetHwRequirement);
}

TEST_F(ArcRevenHardwareCheckerTest, AllHardwareRequirementMet) {
  auto info = mojom::TelemetryInfo::New();
  info->memory_result = CreateMemoryResult(kMemorySizeMeetsRequirementInKiB);
  info->cpu_result = CreateCpuResult(kHasKvmDevice);
  info->block_device_result =
      CreateBlockDeviceResult(kStorageSizeMeetsRequirementInBytes);
  info->bus_result = CreateBusResult(kAllowGpuDeviceId, kAllowWiFiDeviceId);

  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(kMeetHwRequirement);
}

TEST_F(ArcRevenHardwareCheckerTest, BlockDeviceCheckErrorAfterRetries) {
  auto info = ash::cros_healthd::mojom::TelemetryInfo::New();
  info->block_device_result = CreateErrorStorageTag();
  SetFakeTelemetryInfoResponse(std::move(info));
  RunCheckerAndExpect(false);
}

}  // namespace
}  // namespace arc
