// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_applier_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    10 * 1000,       // Initial delay of 10 seconds in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0.2,             // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

}  // namespace

DeviceNameApplierImpl::DeviceNameApplierImpl()
    : DeviceNameApplierImpl(NetworkHandler::Get()->network_state_handler()) {}

DeviceNameApplierImpl::DeviceNameApplierImpl(
    NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler),
      retry_backoff_(&kRetryBackoffPolicy) {}

DeviceNameApplierImpl::~DeviceNameApplierImpl() = default;

void DeviceNameApplierImpl::SetDeviceName(const std::string& new_device_name) {
  device_name_ = new_device_name;
  network_state_handler_->SetHostname(new_device_name);
  ClearRetryAttempts();
  SetBluetoothAdapterName();
}

void DeviceNameApplierImpl::SetBluetoothAdapterName() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&DeviceNameApplierImpl::CallBluetoothAdapterSetName,
                     bluetooth_set_name_weak_factory_.GetWeakPtr()));
}

void DeviceNameApplierImpl::CallBluetoothAdapterSetName(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter->SetName(
      device_name_,
      base::BindOnce(&DeviceNameApplierImpl::OnBluetoothAdapterSetNameSuccess,
                     bluetooth_set_name_weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceNameApplierImpl::OnBluetoothAdapterSetNameError,
                     bluetooth_set_name_weak_factory_.GetWeakPtr()));
}

void DeviceNameApplierImpl::OnBluetoothAdapterSetNameSuccess() {
  VLOG(1) << "Successfully set name in Bluetooth adapter.";
  retry_backoff_.InformOfRequest(/*succeeded=*/true);
}

void DeviceNameApplierImpl::OnBluetoothAdapterSetNameError() {
  retry_backoff_.InformOfRequest(/*succeeded=*/false);
  LOG(WARNING) << "Scheduling setting Bluetooth adapter name to retry in: "
               << retry_backoff_.GetTimeUntilRelease() << " seconds.";

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceNameApplierImpl::SetBluetoothAdapterName,
                     bluetooth_set_name_weak_factory_.GetWeakPtr()),
      retry_backoff_.GetTimeUntilRelease());
}

void DeviceNameApplierImpl::ClearRetryAttempts() {
  // Remove all pending SetBluetoothAdapterName() backoff attempts.
  bluetooth_set_name_weak_factory_.InvalidateWeakPtrs();

  // Reset the state of the backoff so that the next backoff retry starts at
  // the default initial delay.
  retry_backoff_.Reset();
}

}  // namespace ash
