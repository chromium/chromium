// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/scanner_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

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

  QP_LOG(VERBOSE) << __func__ << ": Running saved callbacks.";

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
  QP_LOG(VERBOSE) << __func__ << ": protocol=" << protocol;

  if (!adapter_) {
    QP_LOG(VERBOSE) << __func__
                    << ": No adapter yet, saving callback for later.";

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
  QP_LOG(VERBOSE) << __func__ << ": protocol=" << protocol;

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

  QP_LOG(VERBOSE) << "Starting Fast Pair Scanning.";

  scoped_refptr<FastPairScanner> fast_pair_scanner =
      base::MakeRefCounted<FastPairScannerImpl>();

  fast_pair_discoverable_scanner_ =
      std::make_unique<FastPairDiscoverableScanner>(
          fast_pair_scanner, adapter_,
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceFound,
                              weak_pointer_factory_.GetWeakPtr()),
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceLost,
                              weak_pointer_factory_.GetWeakPtr()));

  fast_pair_not_discoverable_scanner_ =
      std::make_unique<FastPairNotDiscoverableScanner>(
          fast_pair_scanner, adapter_,
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceFound,
                              weak_pointer_factory_.GetWeakPtr()),
          base::BindRepeating(&ScannerBrokerImpl::NotifyDeviceLost,
                              weak_pointer_factory_.GetWeakPtr()));
}

void ScannerBrokerImpl::StopFastPairScanning() {
  fast_pair_discoverable_scanner_.reset();
  fast_pair_not_discoverable_scanner_.reset();
  QP_LOG(VERBOSE) << "Stopping Fast Pair Scanning.";
}

void ScannerBrokerImpl::NotifyDeviceFound(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": device.metadata_id=" << device->metadata_id;

  for (auto& observer : observers_) {
    observer.OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": device.metadata_id=" << device->metadata_id;

  for (auto& observer : observers_) {
    observer.OnDeviceLost(device);
  }
}

}  // namespace quick_pair
}  // namespace ash
