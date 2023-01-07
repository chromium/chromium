// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/mock_scanner_broker.h"

#include "ash/quick_pair/common/device.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

MockScannerBroker::MockScannerBroker() = default;

MockScannerBroker::~MockScannerBroker() = default;

void MockScannerBroker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockScannerBroker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockScannerBroker::NotifyDeviceFound(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnDeviceFound(device);
}

void MockScannerBroker::NotifyDeviceLost(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnDeviceLost(device);
}

}  // namespace quick_pair
}  // namespace ash
