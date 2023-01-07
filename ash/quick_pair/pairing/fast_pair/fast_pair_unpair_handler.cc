// Copyright 2021 The Chromium Authors
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

void FastPairUnpairHandler::DeviceRemoved(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  // TODO(b/235117226): Move this logic to FastPairDelegate implementation for
  // better consistency.
  if (!device || !adapter || device->IsPaired())
    return;

  if (FastPairRepository::Get()->EvictDeviceImages(device)) {
    QP_LOG(VERBOSE) << __func__
                    << ": Repository evicted device images for device = "
                    << device->GetAddress();
  }
}

}  // namespace quick_pair
}  // namespace ash
