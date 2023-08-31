// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/scanner_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner_impl.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

bool ShouldNotDiscoverableScanningBeEnabled(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

ScannerBrokerImpl::ScannerBrokerImpl(QuickPairProcessManager* process_manager)
    : process_manager_(process_manager) {
  DCHECK(process_manager_);
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &ScannerBrokerImpl::OnGetAdapter, weak_pointer_factory_.GetWeakPtr()));
}

ScannerBrokerImpl::~ScannerBrokerImpl() = default;

void ScannerBrokerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  adapter_ = adapter;

  if (start_scanning_on_adapter_callbacks_.empty())
    return;

  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Running saved callbacks.";

  for (auto& callback : start_scanning_on_adapter_callbacks_)
    std::move(callback).Run();

  start_scanning_on_adapter_callbacks_.clear();
}

void ScannerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScannerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScannerBrokerImpl::StartScanning(Protocol protocol) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": protocol=" << protocol;

  if (!adapter_) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": No adapter yet, saving callback for later.";

    start_scanning_on_adapter_callbacks_.push_back(
        base::BindOnce(&ScannerBrokerImpl::StartScanning,
                       weak_pointer_factory_.GetWeakPtr(), protocol));
    return;
  }

  switch (protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      StartFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": protocol=" << protocol;

  switch (protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      StopFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StartFastPairScanning() {
  DCHECK(!fast_pair_discoverable_scanner_);
  DCHECK(!fast_pair_not_discoverable_scanner_);
  DCHECK(adapter_);

  CD_LOG(VERBOSE, Feature::FP) << "Starting Fast Pair Scanning.";

  fast_pair_scanner_ = base::MakeRefCounted<FastPairScannerImpl>();

  fast_pair_discoverable_scanner_ =
      FastPairDiscoverableScannerImpl::Factory::Create(
          fast_pair_scanner_, adapter_,
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceFound,
                              weak_pointer_factory_.GetWeakPtr()),
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceLost,
                              weak_pointer_factory_.GetWeakPtr()));

  // If there is no signed in user, don't instantiate the not discoverable
  // scanner, but observe login events in case that we get logged in later on.
  if (!ShouldNotDiscoverableScanningBeEnabled(
          Shell::Get()->session_controller()->login_status())) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": No logged in user to enable not discoverable scanner";

    // Observe log in events in the case the login was delayed if we aren't
    // observing already.
    if (!shell_observation_.IsObserving())
      shell_observation_.Observe(Shell::Get()->session_controller());

    return;
  }

  fast_pair_not_discoverable_scanner_ =
      FastPairNotDiscoverableScannerImpl::Factory::Create(
          fast_pair_scanner_, adapter_,
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceFound,
                              weak_pointer_factory_.GetWeakPtr()),
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceLost,
                              weak_pointer_factory_.GetWeakPtr()));
}

void ScannerBrokerImpl::OnLoginStatusChanged(LoginStatus login_status) {
  if (!ShouldNotDiscoverableScanningBeEnabled(login_status) ||
      !fast_pair_scanner_ || !adapter_ ||
      fast_pair_not_discoverable_scanner_.get()) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Logged in user, instantiate not discoverable scanner";

  fast_pair_not_discoverable_scanner_ =
      FastPairNotDiscoverableScannerImpl::Factory::Create(
          fast_pair_scanner_, adapter_,
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceFound,
                              weak_pointer_factory_.GetWeakPtr()),
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceLost,
                              weak_pointer_factory_.GetWeakPtr()));
}

void ScannerBrokerImpl::StopFastPairScanning() {
  fast_pair_discoverable_scanner_.reset();
  fast_pair_not_discoverable_scanner_.reset();
  fast_pair_scanner_.reset();
  shell_observation_.Reset();
  CD_LOG(VERBOSE, Feature::FP) << "Stopping Fast Pair Scanning.";
}

void ScannerBrokerImpl::NotifyDeviceFound(scoped_refptr<Device> device) {
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": device.metadata_id=" << device->metadata_id();

  for (auto& observer : observers_) {
    observer.OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(scoped_refptr<Device> device) {
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": device.metadata_id=" << device->metadata_id();

  for (auto& observer : observers_) {
    observer.OnDeviceLost(device);
  }
}

void ScannerBrokerImpl::OnDevicePaired(scoped_refptr<Device> device) {
  if (fast_pair_scanner_)
    fast_pair_scanner_->OnDevicePaired(device);
}

}  // namespace quick_pair
}  // namespace ash
