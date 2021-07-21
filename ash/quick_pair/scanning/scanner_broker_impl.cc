// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/scanner_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace quick_pair {

ScannerBrokerImpl::ScannerBrokerImpl() {
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

  QP_LOG(INFO) << __func__ << ": Running saved callbacks.";

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
  QP_LOG(INFO) << __func__ << ": protocol=" << protocol;

  if (!adapter_) {
    QP_LOG(INFO) << __func__ << ": No adapter yet, saving callback for later.";

    start_scanning_on_adapter_callbacks_.push_back(
        base::BindOnce(&ScannerBrokerImpl::StartScanning,
                       weak_pointer_factory_.GetWeakPtr(), protocol));
    return;
  }

  switch (protocol) {
    case Protocol::kFastPair:
      StartFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
  QP_LOG(INFO) << __func__ << ": protocol=" << protocol;

  switch (protocol) {
    case Protocol::kFastPair:
      StopFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StartFastPairScanning() {
  QP_LOG(INFO) << "Starting Fast Pair Scanning.";
}

void ScannerBrokerImpl::StopFastPairScanning() {
  QP_LOG(INFO) << "Stoping Fast Pair Scanning.";
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
