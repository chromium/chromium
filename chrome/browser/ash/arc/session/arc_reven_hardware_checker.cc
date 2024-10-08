// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_reven_hardware_checker.h"

#include <iomanip>

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

namespace mojom = ash::cros_healthd::mojom;
namespace {
// Converts a 16-bit integer to a zero-padded hexadecimal string.
std::string IntToHex(uint16_t value) {
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(4) << std::hex << value;
  return stream.str();
}

}  // namespace

// Minimum required memory to enable the arcvm: 4 GB in KiB.
constexpr int64_t kMinMemorySizeInKiB = 4LL * 1024LL * 1024LL;
// Minimum required storage to enable the arcvm: 32 GB in bytes.
constexpr int64_t kMinStorageSizeInBytes = 32LL * 1024LL * 1024LL * 1024LL;

// It takes hundreds of milliseconds to wait for the hardware
// information to be ready, so set the timeout to five seconds (100 ms * 50)
// as a safe interval.
constexpr int64_t kMaxRetries = 50;
constexpr base::TimeDelta kHardwareInfoReadyRetryInterval =
    base::Milliseconds(100);

const std::unordered_set<std::string>
    ArcRevenHardwareChecker::kSupportedWiFiIds{
        "8086:095a", "8086:a0f0", "8086:2526", "8086:31dc", "8086:9df0",
        "8086:51f0", "168c:003e", "8086:2723", "8086:06f0", "10ec:c822",
        "8086:7af0", "8086:4df0", "8086:2725", "8086:095b", "14c3:7961",
        "8086:a370", "14c3:0616", "10ec:8852", "8086:43f0"};

const std::unordered_set<std::string> ArcRevenHardwareChecker::kSupportedGpuIds{
    "8086:9a49", "8086:9a78", "8086:9a60", "8086:9a40", "8086:9a70",
    "8086:9a68", "8086:9a59", "8086:9af8", "8086:9ad9", "8086:9ac9",
    "8086:9ac0", "8086:a780", "8086:a781", "8086:a782", "8086:a783",
    "8086:a788", "8086:a789", "8086:a78a", "8086:a78b", "8086:a7a9",
    "8086:a721", "8086:a7a1", "8086:a720", "8086:a7a8", "8086:a7a0",
    "8086:5917", "8086:5916", "8086:5912", "8086:591e", "8086:5921",
    "8086:5906", "8086:591c", "8086:5926", "8086:593b", "8086:5923",
    "8086:5927", "8086:591b", "8086:591d", "8086:591a", "8086:87c0",
    "8086:5915", "8086:5913", "8086:590b", "8086:5902", "8086:590e",
    "8086:5908", "8086:590a", "8086:4e61", "8086:4e55", "8086:4e71",
    "8086:4e51", "8086:4e57", "8086:3185", "8086:3184", "8086:3ea0",
    "8086:9b41", "8086:3e92", "8086:9bc8", "8086:3e91", "8086:9ba8",
    "8086:9bc5", "8086:3ea5", "8086:3e90", "8086:9bc4", "8086:3ea9",
    "8086:3e9b", "8086:9bca", "8086:3e98", "8086:9b21", "8086:9baa",
    "8086:3ea8", "8086:3ea6", "8086:3ea7", "8086:3ea2", "8086:3ba5",
    "8086:3ea1", "8086:3e9c", "8086:3e99", "8086:3e93", "8086:9bac",
    "8086:9bab", "8086:9ba4", "8086:9ba2", "8086:9ba0", "8086:9ea4",
    "8086:9bcc", "8086:9bcb", "8086:9bc2", "8086:9bc0", "8086:3ea3",
    "8086:87ca", "8086:9bf6", "8086:9be6", "8086:9bc6", "8086:3e94",
    "8086:3e9a", "8086:3e96", "1002:15e7", "8086:4692", "8086:4690",
    "8086:4693", "8086:4682", "8086:4680", "8086:468b", "8086:468a",
    "8086:4688", "8086:46d1", "8086:46d0", "8086:46d2", "8086:46a8",
    "8086:46b3", "8086:4628", "8086:46a6", "8086:46c3", "8086:46a3",
    "8086:46a2", "8086:46a1", "8086:46a0", "8086:462a", "8086:46b2",
    "8086:46b1", "8086:46b0", "8086:46aa", "8086:4626", "1002:15d8",
    "1002:1638"};

ArcRevenHardwareChecker::ArcRevenHardwareChecker() {}
ArcRevenHardwareChecker::~ArcRevenHardwareChecker() {}

void ArcRevenHardwareChecker::IsRevenDeviceCompatibleForArc(
    base::OnceCallback<void(bool)> callback) {
  if (!probe_service_ || !probe_service_.is_connected()) {
    ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(base::BindOnce(
        &ArcRevenHardwareChecker::OnDisconnect, weak_factory_.GetWeakPtr()));
  }
  // Check whether the hardware information is ready.
  probe_service_->ProbeTelemetryInfo(
      {mojom::ProbeCategoryEnum::kNonRemovableBlockDevices},
      base::BindOnce(&ArcRevenHardwareChecker::OnCheckNonRemovableBlockDevices,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcRevenHardwareChecker::OnCheckNonRemovableBlockDevices(
    base::OnceCallback<void(bool)> callback,
    mojom::TelemetryInfoPtr info_ptr) {
  // Successfully obtained block device information.
  if (!info_ptr.is_null() && info_ptr->block_device_result &&
      info_ptr->block_device_result->which() ==
          ash::cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::
              kBlockDeviceInfo) {
    if (retry_timer_.IsRunning()) {
      retry_timer_.Stop();
      retry_count_ = 0;
    }
    base::UmaHistogramBoolean("Arc.RevenHardwareChecker.Timeout", false);
    // Request telemetry information from the cros_healthd service.
    probe_service_->ProbeTelemetryInfo(
        {mojom::ProbeCategoryEnum::kCpu,
         mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
         mojom::ProbeCategoryEnum::kMemory, mojom::ProbeCategoryEnum::kBus},
        base::BindOnce(&ArcRevenHardwareChecker::OnRevenHardwareChecked,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Timeout to obtain block device information.
  if (retry_count_ > kMaxRetries) {
    base::UmaHistogramBoolean("Arc.RevenHardwareChecker.Timeout", true);
    LOG(ERROR)
        << "Did not wait for hardware information to be ready before timeout";
    std::move(callback).Run(false);
    retry_timer_.Stop();
    retry_count_ = 0;
    return;
  }

  // Retry to obtain the block device information.
  retry_count_++;
  retry_timer_.Start(
      FROM_HERE, kHardwareInfoReadyRetryInterval,
      base::BindOnce(
          &ArcRevenHardwareChecker::OnRetryNonRemovableBlockDevicesCheck,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcRevenHardwareChecker::OnRetryNonRemovableBlockDevicesCheck(
    base::OnceCallback<void(bool)> callback) {
  probe_service_->ProbeTelemetryInfo(
      {mojom::ProbeCategoryEnum::kNonRemovableBlockDevices},
      base::BindOnce(&ArcRevenHardwareChecker::OnCheckNonRemovableBlockDevices,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcRevenHardwareChecker::OnDisconnect() {
  probe_service_.reset();
}

void ArcRevenHardwareChecker::OnRevenHardwareChecked(
    base::OnceCallback<void(bool)> callback,
    mojom::TelemetryInfoPtr info_ptr) {
  if (info_ptr.is_null()) {
    LOG(WARNING)
        << "Received null response from croshealthd::ProbeTelemetryInfo";
    std::move(callback).Run(false);
    return;
  }

  // Check if all hardware requirements are met.
  bool is_compatible =
      CheckMemoryRequirements(info_ptr) && CheckCpuRequirements(info_ptr) &&
      CheckStorageRequirements(info_ptr) && CheckPciRequirements(info_ptr);

  std::move(callback).Run(is_compatible);
}

bool ArcRevenHardwareChecker::CheckMemoryRequirements(
    const mojom::TelemetryInfoPtr& info_ptr) const {
  if (!info_ptr->memory_result) {
    LOG(WARNING) << "No memory result in response from cros_healthd.";
    return false;
  }
  const auto& memory_info = info_ptr->memory_result->get_memory_info();
  if (!memory_info) {
    LOG(WARNING) << "No memory info in response from cros_healthd.";
    return false;
  }
  return memory_info->total_memory_kib >= kMinMemorySizeInKiB;
}

bool ArcRevenHardwareChecker::CheckCpuRequirements(
    const mojom::TelemetryInfoPtr& info_ptr) const {
  if (!info_ptr->cpu_result) {
    LOG(WARNING) << "No CPU result in response from cros_healthd.";
    return false;
  }
  const auto& cpu_info = info_ptr->cpu_result->get_cpu_info();
  if (!cpu_info) {
    LOG(WARNING) << "No CPU info in response from cros_healthd.";
    return false;
  }

  return cpu_info->virtualization && cpu_info->virtualization->has_kvm_device;
}

bool ArcRevenHardwareChecker::CheckStorageRequirements(
    const mojom::TelemetryInfoPtr& info_ptr) const {
  if (!info_ptr->block_device_result) {
    LOG(WARNING) << "No block device result in response from cros_healthd.";
    return false;
  }
  const auto& block_devices_info =
      info_ptr->block_device_result->get_block_device_info();
  if (block_devices_info.empty()) {
    LOG(WARNING)
        << "No non-removable block devices in response from cros_healthd.";
    return false;
  }

  // Check for a suitable boot device with minimum storage size and that is not
  // a spinning HDD.
  for (const auto& device : block_devices_info) {
    if (device->purpose == mojom::StorageDevicePurpose::kBootDevice &&
        device->size >= kMinStorageSizeInBytes &&
        device->is_rotational.has_value() && !device->is_rotational.value()) {
      return true;
    }
  }

  LOG(WARNING)
      << "No suitable boot device found among non-removable block devices.";
  return false;
}

bool ArcRevenHardwareChecker::CheckPciRequirements(
    const mojom::TelemetryInfoPtr& info_ptr) const {
  if (!info_ptr->bus_result) {
    LOG(WARNING) << "No bus result in response from cros_healthd.";
    return false;
  }

  const auto& bus_devices = info_ptr->bus_result->get_bus_devices();
  if (bus_devices.empty()) {
    LOG(WARNING) << "No bus devices in response from cros_healthd.";
    return false;
  }
  bool is_wifi_compatible = false;
  bool is_gpu_compatible = false;

  for (const auto& device : bus_devices) {
    if (device->bus_info->which() == mojom::BusInfo::Tag::kPciBusInfo) {
      const auto& pci_info = device->bus_info->get_pci_bus_info();
      const auto& pci_id =
          IntToHex(pci_info->vendor_id) + ":" + IntToHex(pci_info->device_id);

      if (device->device_class == mojom::BusDeviceClass::kWirelessController &&
          kSupportedWiFiIds.find(pci_id) != kSupportedWiFiIds.end()) {
        is_wifi_compatible = true;
      }
      if (device->device_class == mojom::BusDeviceClass::kDisplayController &&
          kSupportedGpuIds.find(pci_id) != kSupportedGpuIds.end()) {
        is_gpu_compatible = true;
      }
    }
  }
  return is_wifi_compatible && is_gpu_compatible;
}

}  // namespace arc
