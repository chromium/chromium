// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/scanner_broker_impl.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"

namespace ash {
namespace quick_pair {

ScannerBrokerImpl::ScannerBrokerImpl() = default;

ScannerBrokerImpl::~ScannerBrokerImpl() = default;

void ScannerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScannerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScannerBrokerImpl::StartScanning(Protocol protocol) {
  switch (protocol) {
    case Protocol::kFastPair:
      StartFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
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

void ScannerBrokerImpl::NotifyDeviceFound(const Device& device) {
  for (auto& observer : observers_) {
    observer.OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(const Device& device) {
  for (auto& observer : observers_) {
    observer.OnDeviceLost(device);
  }
}

}  // namespace quick_pair
}  // namespace ash
