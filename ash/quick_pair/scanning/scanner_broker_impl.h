// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_

#include "ash/quick_pair/scanning/scanner_broker.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

struct Device;

class ScannerBrokerImpl : public ScannerBroker {
 public:
  ScannerBrokerImpl();
  ScannerBrokerImpl(const ScannerBrokerImpl&) = delete;
  ScannerBrokerImpl& operator=(const ScannerBrokerImpl&) = delete;
  ~ScannerBrokerImpl() override;

  // ScannerBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void StartScanning(Protocol protocol) override;
  void StopScanning(Protocol protocol) override;

 private:
  void StartFastPairScanning();
  void StopFastPairScanning();

  void NotifyDeviceFound(scoped_refptr<Device> device);
  void NotifyDeviceLost(scoped_refptr<Device> device);

  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
