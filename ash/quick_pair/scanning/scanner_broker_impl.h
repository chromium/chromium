// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_

#include <memory>
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

class FastPairDiscoverableScanner;
class FastPairNotDiscoverableScanner;
class Device;
class QuickPairProcessManager;

class ScannerBrokerImpl : public ScannerBroker, public SessionObserver {
 public:
  explicit ScannerBrokerImpl(QuickPairProcessManager* process_manager);
  ScannerBrokerImpl(const ScannerBrokerImpl&) = delete;
  ScannerBrokerImpl& operator=(const ScannerBrokerImpl&) = delete;
  ~ScannerBrokerImpl() override;

  // ScannerBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void StartScanning(Protocol protocol) override;
  void StopScanning(Protocol protocol) override;
  void OnDevicePaired(scoped_refptr<Device> device) override;

 private:
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void StartFastPairScanning();
  void StopFastPairScanning();

  void NotifyDeviceFound(scoped_refptr<Device> device);
  void NotifyDeviceLost(scoped_refptr<Device> device);

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<QuickPairProcessManager, DanglingUntriaged> process_manager_ =
      nullptr;
  std::vector<base::OnceClosure> start_scanning_on_adapter_callbacks_;
  scoped_refptr<FastPairScanner> fast_pair_scanner_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<FastPairDiscoverableScanner> fast_pair_discoverable_scanner_;
  std::unique_ptr<FastPairNotDiscoverableScanner>
      fast_pair_not_discoverable_scanner_;
  base::ObserverList<Observer> observers_;
  base::ScopedObservation<SessionController, SessionObserver>
      shell_observation_{this};
  base::WeakPtrFactory<ScannerBrokerImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
