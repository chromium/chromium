// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_unpair_handler.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

FastPairUnpairHandler::FastPairUnpairHandler(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(std::move(adapter)) {
  observation_.Observe(adapter_.get());
}

FastPairUnpairHandler::~FastPairUnpairHandler() = default;

void FastPairUnpairHandler::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetAddress()
                  << " new_paired_status="
                  << (new_paired_status ? "true" : "false");

  if (new_paired_status)
    return;

  if (FastPairRepository::Get()->DeleteAssociatedDevice(device)) {
    QP_LOG(INFO) << __func__ << ": Repository is processing the delete";
  } else {
    QP_LOG(VERBOSE) << __func__ << ": No device found by repository";
  }
}

}  // namespace quick_pair
}  // namespace ash
