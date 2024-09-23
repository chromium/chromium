// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cros_healthd_metrics_provider.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using metrics::SystemProfileProto;

namespace {

constexpr base::TimeDelta kServiceDiscoveryTimeout = base::Seconds(5);

}  // namespace

CrosHealthdMetricsProvider::CrosHealthdMetricsProvider() = default;
CrosHealthdMetricsProvider::~CrosHealthdMetricsProvider() = default;

base::TimeDelta CrosHealthdMetricsProvider::GetTimeout() {
  return kServiceDiscoveryTimeout;
}

void CrosHealthdMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  const std::vector<ash::cros_healthd::mojom::ProbeCategoryEnum>
      categories_to_probe = {ash::cros_healthd::mojom::ProbeCategoryEnum::
                                 kNonRemovableBlockDevices};
  DCHECK(init_callback_.is_null());
  init_callback_ = std::move(done_callback);
  initialized_ = false;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CrosHealthdMetricsProvider::OnProbeTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      GetTimeout());
  GetService()->ProbeTelemetryInfo(
      categories_to_probe,
      base::BindOnce(&CrosHealthdMetricsProvider::OnProbeDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool CrosHealthdMetricsProvider::IsInitialized() {
  return initialized_;
}

void CrosHealthdMetricsProvider::OnProbeTimeout() {
  base::ScopedClosureRunner runner(std::move(init_callback_));
  DVLOG(1) << "cros_healthd: endpoint is not found.";

  // Invalidate OnProbeDone callback.
  weak_ptr_factory_.InvalidateWeakPtrs();

  devices_.clear();
  initialized_ = false;
}

void CrosHealthdMetricsProvider::OnProbeDone(
    ash::cros_healthd::mojom::TelemetryInfoPtr ptr) {
  base::ScopedClosureRunner runner(std::move(init_callback_));

  // Invalidate OnProbeTimeout callback.
  weak_ptr_factory_.InvalidateWeakPtrs();

  devices_.clear();
  initialized_ = true;

  if (ptr.is_null()) {
    DVLOG(1) << "cros_healthd: Empty response";
    return;
  }

  const auto& block_device_result = ptr->block_device_result;
  if (block_device_result.is_null()) {
    DVLOG(1) << "cros_healthd: No block device info";
    return;
  }

  auto tag = block_device_result->which();
  if (tag ==
      ash::cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::kError) {
    DVLOG(1) << "cros_healthd: Error getting block device info: "
             << block_device_result->get_error()->msg;
    return;
  }
  DCHECK_EQ(tag, ash::cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::
                     kBlockDeviceInfo);

  for (const auto& storage : block_device_result->get_block_device_info()) {
    SystemProfileProto::Hardware::InternalStorageDevice dev;
    const auto& device_info = storage->device_info;
    if (device_info.is_null()) {
      DVLOG(1)
          << "cros_healthd: No device info in block device info for storage: "
          << storage->name;
      continue;
    }

    switch (device_info->which()) {
      case ash::cros_healthd::mojom::BlockDeviceInfo::Tag::kUnrecognized: {
        // Skip reporting entries for the unknown types.
        continue;
      }

      case ash::cros_healthd::mojom::BlockDeviceInfo::Tag::kNvmeDeviceInfo: {
        dev.set_type(
            SystemProfileProto::Hardware::InternalStorageDevice::TYPE_NVME);
        dev.set_vendor_id(
            device_info->get_nvme_device_info()->subsystem_vendor);
        dev.set_product_id(
            device_info->get_nvme_device_info()->subsystem_device);
        dev.set_revision(device_info->get_nvme_device_info()->pcie_rev);
        dev.set_firmware_version(
            device_info->get_nvme_device_info()->firmware_rev);
        break;
      }

      case ash::cros_healthd::mojom::BlockDeviceInfo::Tag::kEmmcDeviceInfo: {
        dev.set_type(
            SystemProfileProto::Hardware::InternalStorageDevice::TYPE_EMMC);
        dev.set_vendor_id(device_info->get_emmc_device_info()->manfid);
        dev.set_product_id(device_info->get_emmc_device_info()->pnm);
        dev.set_revision(device_info->get_emmc_device_info()->prv);
        dev.set_firmware_version(device_info->get_emmc_device_info()->fwrev);
        break;
      }

      case ash::cros_healthd::mojom::BlockDeviceInfo::Tag::kUfsDeviceInfo: {
        dev.set_type(
            SystemProfileProto::Hardware::InternalStorageDevice::TYPE_UFS);
        dev.set_vendor_id(device_info->get_ufs_device_info()->jedec_manfid);
        // UFS does not provide numeric id. Use hashed model name instead.
        dev.set_product_id(base::PersistentHash(storage->name));
        dev.set_firmware_version(device_info->get_ufs_device_info()->fwrev);
        break;
      }
    }

    switch (storage->purpose) {
      case ash::cros_healthd::mojom::StorageDevicePurpose::kUnknown:
        dev.set_purpose(SystemProfileProto::Hardware::InternalStorageDevice::
                            PURPOSE_UNKNOWN);
        break;
      case ash::cros_healthd::mojom::StorageDevicePurpose::kBootDevice:
        dev.set_purpose(
            SystemProfileProto::Hardware::InternalStorageDevice::PURPOSE_BOOT);
        break;
      case ash::cros_healthd::mojom::StorageDevicePurpose::
          DEPRECATED_kSwapDevice:
        dev.set_purpose(
            SystemProfileProto::Hardware::InternalStorageDevice::PURPOSE_SWAP);
        break;
      case ash::cros_healthd::mojom::StorageDevicePurpose::kNonBootDevice:
        break;
    }

    dev.set_model(storage->name);
    dev.set_size_mb(storage->size / 1e6);

    devices_.push_back(dev);
  }
}

ash::cros_healthd::mojom::CrosHealthdProbeService*
CrosHealthdMetricsProvider::GetService() {
  if (!service_ || !service_.is_connected()) {
    ash::cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(
        base::BindOnce(&CrosHealthdMetricsProvider::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return service_.get();
}

void CrosHealthdMetricsProvider::OnDisconnect() {
  service_.reset();
}

void CrosHealthdMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  if (!initialized_) {
    return;
  }
  auto* mutable_hardware_proto = system_profile_proto->mutable_hardware();
  mutable_hardware_proto->clear_internal_storage_devices();

  for (const auto& device_info_mojo : devices_) {
    auto* device_info_uma =
        mutable_hardware_proto->add_internal_storage_devices();
    device_info_uma->MergeFrom(device_info_mojo);
  }
}
