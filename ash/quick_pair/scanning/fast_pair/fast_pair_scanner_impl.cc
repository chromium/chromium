// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include "device/bluetooth/bluetooth_adapter_factory.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;

namespace ash {
namespace quick_pair {

FastPairScannerImpl::FastPairScannerImpl() {
  BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FastPairScannerImpl::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

FastPairScannerImpl::~FastPairScannerImpl() = default;

void FastPairScannerImpl::AddObserver(FastPairScanner::Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairScannerImpl::RemoveObserver(FastPairScanner::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairScannerImpl::DeviceAdded(BluetoothAdapter* adapter,
                                      BluetoothDevice* device) {
  for (auto& observer : observers_)
    observer.OnDeviceFound(device);
}

void FastPairScannerImpl::DeviceRemoved(BluetoothAdapter* adapter,
                                        BluetoothDevice* device) {
  for (auto& observer : observers_)
    observer.OnDeviceLost(device);
}

void FastPairScannerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());
}

}  // namespace quick_pair
}  // namespace ash
